/*
	Analyzer for WHERE condition: find timestamp intervals.
*/

#define MYSQL_SERVER 1
#include "sql/table.h"
#include "sql/item.h"
#include "sql/item_func.h"
#include "sql/item_cmpfunc.h"
#include "sql/current_thd.h"
#include "sql/sql_time.h"
#include "sql/sql_class.h"

#include "../handler/plugin.h"		// For configuration parameters.
#include "condition.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// TimePeriods
//////////////////////////////////////////////////////////////////////////////////////////////////////

void TimePeriods::makeAnd(const TimePeriods& right) {
	TimePeriods result(std::max(capacity(), right.capacity()));
	for (uint32_t i = 0; i < right.length(); ++i) {
		const TimePeriod& pright = right[i];
		for (uint32_t j = 0; j < length(); ++j) {
			const TimePeriod& pleft = (*this)[j];
			result.insert(pleft.makeIntersection(pright));
		}
	}
	*this = result;
	if (isEmpty()) {
		insert(TimePeriod());
	}
	compact();
}

void TimePeriods::makeOr(const TimePeriod& right) {
	bool found = false;
	for (uint32_t j = 0; j < length(); ++j) {
		const TimePeriod& left = (*this)[j];
		if (left.intersects(right) || left.isAdjacent(right)) {
			remove(left);
			insert(left.makeUnion(right));
			found = true;
			break;
		}
	}
	if (!found) {
		insert(right);
	}
	compact();
}

void TimePeriods::makeOr(const TimePeriods& right) {
	for (uint32_t i = 0; i < right.length(); ++i) {
		makeOr(right[i]);
	}
}

void TimePeriods::makeNot() {
	TimePeriods list(*this);
	clear();
	TimePeriod tmp[2];
	for (uint32_t i = 0; i < list.length(); ++i) {
		list[i].makeNot(tmp);
		const int n = tmp[1].isVoid() ? 1 : 2;
		for (int j = 0; j < n; ++j) {
			makeOr(tmp[j]);
		}
	}
}

void TimePeriods::compact() {
	uint32_t i = 0;
	while (i + 1 < length()) {
		TimePeriod& p1 = (*this)[i];
		TimePeriod& p2 = (*this)[i + 1];
		if (p1.intersects(p2)) {
			p1 = p1.makeUnion(p2);
			removeAt(i + 1);
		} else {
			i++;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Condition
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Timestamp pruning: browses pushed condition and returns a list of timestamp intervals.
// If we end up with ]-inf, +inf[, use given lower timestamp to restrict time range.
// STATIC
TimePeriods Condition::getPeriods(TABLE* table, Item* cond, const uint64_t lower) {
	SPARROW_ENTER("Condition::getPeriods");
	TimePeriods periods;
	if (cond != 0) {
		periods = Condition::get(table, cond, true);
	}
	if (periods.isEmpty() || periods.first().isAll()) {
		periods.clear();
		if (lower == 0) {
			periods.insert(TimePeriod());
		} else {
			periods.insert(TimePeriod(&lower, 0, true, false));
		}
	}
	return periods;
}

// STATIC
// See Condition::getPeriods()
TimePeriods Condition::get(TABLE* table, Item* item, const bool returnAllIfNone) {
	SPARROW_ENTER("Condition::get");
	TimePeriods periods;
	switch (item->type()) {
		case Item::FIELD_ITEM:
			break;
		case Item::FUNC_ITEM:
		case Item::COND_ITEM: {
			Item ** args = NULL;
			int tindex = -1;
			bool isTimestamp = true;
			Item_func* func = static_cast<Item_func*>(item);
			const uint n = func->argument_count();
			if ( n != 0 ) {
				args = func->arguments();
				for (uint i = 0; i < n; ++i) {
					const Item* arg = args[i];
					if (arg->type() == Item::FIELD_ITEM) {
						const Item_field* field = static_cast<const Item_field*>(arg);
						if (field->field->table == table && field->field->field_index() == 0) {
							tindex = i;
							break;
						}
					} else if (arg->type() == Item::FUNC_ITEM) {
						const Item_func* test = static_cast<const Item_func*>(arg);
						if (strcmp(test->func_name(), "unix_timestamp") == 0 && test->argument_count() == 1) {
							const Item* testArg = test->arguments()[0];
							if (testArg->type() == Item::FIELD_ITEM) {
								const Item_field* field = static_cast<const Item_field*>(testArg);
								if (field->field->table == table && field->field->field_index() == 0) {
									tindex = i;
									isTimestamp = false;
									break;
								}
							}
						}
					}
				}
			}
			uint64_t bound = (uint64_t)-1;
			if (tindex >= 0 && n >= 2) {
				bound = getBound(args[tindex == 0 ? 1 : 0], isTimestamp);
			}

			// Since MySQL timestamps have no milliseconds, in some cases, we add 999ms to
			// the lower or upper bound of the timestamp interval to make sure 
			// we get all matching records.
 			switch (func->functype()) {
				case Item_func::EQUAL_FUNC:
				case Item_func::EQ_FUNC: {
					if (bound != (uint64_t)-1) {
#ifndef NDEBUG
						const Str stime(Str::fromTimestamp(bound));
						DBUG_PRINT("sparrow_context", ("= %s", stime.c_str()));
#endif
						periods.insert(TimePeriod(bound));
					}
					break;
				}
				case Item_func::MULT_EQUAL_FUNC: {
					Item_equal* equal = static_cast<Item_equal*>(func);
					const Item* constItem = equal->const_arg();
					if (constItem != 0) {
						const Item_field* field = equal->get_first();
						if (field != 0 && field->field->field_index() == 0) {
							bound = getBound(equal->const_arg(), isTimestamp);
#ifndef NDEBUG
							const Str stime(Str::fromTimestamp(bound));
							DBUG_PRINT("sparrow_context", ("= %s", stime.c_str()));
#endif
							periods.insert(TimePeriod(bound));
						}
					}
					break;
				}
				case Item_func::NE_FUNC: {
					if (bound != (uint64_t)-1) {
#ifndef NDEBUG
						const Str stime(Str::fromTimestamp(bound));
						DBUG_PRINT("sparrow_context", ("= %s", stime.c_str()));
#endif
						periods.insert(TimePeriod(bound));
						periods.makeNot();
					}
					break;
				}
				case Item_func::LT_FUNC: {
					if (bound != (uint64_t)-1) {
#ifndef NDEBUG
						const Str stime(Str::fromTimestamp(bound));
						DBUG_PRINT("sparrow_context", ("< %s", stime.c_str()));
#endif
						if ( tindex == 0 ) {
							periods.insert(TimePeriod(0, &bound, false, false));
						} else {
							periods.insert(TimePeriod(&bound, 0, false, false));
						}
					}
					break;
				}
				case Item_func::LE_FUNC: {
					if (bound != (uint64_t)-1) {
#ifndef NDEBUG
						const Str stime(Str::fromTimestamp(bound));
						DBUG_PRINT("sparrow_context", ("<= %s", stime.c_str()));
#endif
						if ( tindex == 0 ) {
							periods.insert(TimePeriod(0, &bound, false, true));
						} else {
							periods.insert(TimePeriod(&bound, 0, true, false));
						}
					}
					break;
				}
				case Item_func::GE_FUNC: {
					if (bound != (uint64_t)-1) {
#ifndef NDEBUG
						const Str stime(Str::fromTimestamp(bound));
						DBUG_PRINT("sparrow_context", (">= %s", stime.c_str()));
#endif
						if ( tindex == 0 ) {
							periods.insert(TimePeriod(&bound, 0, true, false));
						} else {
							periods.insert(TimePeriod(0, &bound, false, true));
						}

					}
					break;
				}
				case Item_func::GT_FUNC: {
					if (bound != (uint64_t)-1) {
#ifndef NDEBUG
						const Str stime(Str::fromTimestamp(bound));
						DBUG_PRINT("sparrow_context", ("> %s", stime.c_str()));
#endif
						if ( tindex == 0 ) {
							periods.insert(TimePeriod(&bound, 0, false, false));
						} else {
							periods.insert(TimePeriod(0, &bound, false, false));
						}
					}
					break;
				}
				case Item_func::COND_AND_FUNC: {
					List<Item>* list = static_cast<Item_cond*>(func)->argument_list();
					List_iterator<Item> li(*list);
					Item* iitem;
					while ((iitem = li++) != 0) {
						if (periods.isEmpty()) {
							periods = Condition::get(table, iitem, true);
						} else {
							DBUG_PRINT("sparrow_context", ("AND"));
							periods.makeAnd(Condition::get(table, iitem, true));
						}
					}
					break;
				}
				case Item_func::COND_OR_FUNC: {
					List<Item>* list = static_cast<Item_cond*>(func)->argument_list();
					List_iterator<Item> li(*list);
					Item* iitem;
					while ((iitem = li++) != 0) {
						if (periods.isEmpty()) {
							periods = Condition::get(table, li++, true);
						} else {
							DBUG_PRINT("sparrow_context", ("OR"));
							periods.makeOr(Condition::get(table, iitem, true));
						}
					}
					break;
				}
				case Item_func::NOT_FUNC: {
					DBUG_PRINT("sparrow_context", ("NOT"));
					periods = Condition::get(table, func->arguments()[0], false);
					break;
				}
				case Item_func::BETWEEN: {
					if (bound != (uint64_t)-1) {
						const uint64_t bound2 = getBound(args[2], isTimestamp);
						if (bound2 != 0) {
							TimePeriod	period = TimePeriod(bound, bound2);
#ifndef NDEBUG
							const Str speriod(Str::fromTimePeriod(period));
							DBUG_PRINT("sparrow_context", ("BETWEEN %s", speriod.c_str()));
#endif
							periods.insert(period);
						}
					}
					break;
				}
				case Item_func::IN_FUNC: {
					if (bound != (uint64_t)-1) {
						periods.insert(TimePeriod(bound));
#ifndef NDEBUG
						TimePeriod	period(bound);
						const Str speriod(Str::fromTimePeriod(period));
						DBUG_PRINT("sparrow_context", ("IN %s", speriod.c_str()));
#endif
						for (int i = 2; i < static_cast<int>(n); ++i) {
							if (i != tindex) {
								const uint64_t other = getBound(args[i], isTimestamp);
								if (other != 0) {
									periods.insert(TimePeriod(other));
#ifndef NDEBUG
									TimePeriod	period(other);
									const Str speriod(Str::fromTimePeriod(period));
									DBUG_PRINT("sparrow_context", ("IN %s", speriod.c_str()));
#endif
								}
							}
						}
					}
					break;
				}
				case Item_func::FT_FUNC:
				case Item_func::UNKNOWN_FUNC:
				case Item_func::LIKE_FUNC:
				case Item_func::ISNULL_FUNC:
				case Item_func::ISNOTNULL_FUNC:
				//case Item_func::COND_XOR_FUNC:
				case Item_func::ISNOTNULLTEST_FUNC:
				case Item_func::SP_EQUALS_FUNC:
				case Item_func::SP_DISJOINT_FUNC:
				case Item_func::SP_INTERSECTS_FUNC:
				case Item_func::SP_TOUCHES_FUNC:
				case Item_func::SP_CROSSES_FUNC:
				case Item_func::SP_WITHIN_FUNC:
				case Item_func::SP_CONTAINS_FUNC:
				case Item_func::SP_OVERLAPS_FUNC:
				case Item_func::SP_STARTPOINT:
				case Item_func::SP_ENDPOINT:
				case Item_func::SP_EXTERIORRING:
				case Item_func::SP_POINTN:
				case Item_func::SP_GEOMETRYN:
				case Item_func::SP_INTERIORRINGN:
				case Item_func::NOT_ALL_FUNC:
				case Item_func::NOW_FUNC:
				case Item_func::TRIG_COND_FUNC:
				case Item_func::SUSERVAR_FUNC:
				case Item_func::EXTRACT_FUNC:
				case Item_func::TYPECAST_FUNC:
				case Item_func::FUNC_SP:
				case Item_func::UDF_FUNC:
				default: break;
			}
			break;
		}
		case Item::SUM_FUNC_ITEM:
		case Item::STRING_ITEM:
		case Item::INT_ITEM:
		case Item::REAL_ITEM:
		case Item::NULL_ITEM:
		case Item::VARBIN_ITEM:
		case Item::METADATA_COPY_ITEM:
		case Item::FIELD_AVG_ITEM:
		case Item::DEFAULT_VALUE_ITEM:
		case Item::PROC_ITEM:
		case Item::REF_ITEM:
		case Item::FIELD_STD_ITEM:
		case Item::FIELD_VARIANCE_ITEM:
		case Item::INSERT_VALUE_ITEM:
		case Item::SUBSELECT_ITEM:
		case Item::ROW_ITEM:
		case Item::CACHE_ITEM:
		case Item::TYPE_HOLDER:
		case Item::PARAM_ITEM:
		case Item::TRIGGER_FIELD_ITEM:
		case Item::DECIMAL_ITEM:
		case Item::XPATH_NODESET:
		case Item::XPATH_NODESET_CMP:
		case Item::VIEW_FIXER_ITEM:
		default: break;
	}
	if (returnAllIfNone && periods.isEmpty()) {
		// Nothing found: return ]-inf, +inf[.
		periods.insert(TimePeriod());
	}
	return periods;
}

// STATIC
uint64_t Condition::getBound(Item* item, const bool isTimestamp) {
	Item_result	res_type = item->result_type();
	if (res_type == INT_RESULT) {
		if (item->val_int() < 0) return -1;
		return item->val_int() * 1000ULL;
	} else if (res_type == STRING_RESULT) {
		MYSQL_TIME t;
		if (item->get_date(&t, TIME_FUZZY_DATE) == 0) {
			THD* thd = current_thd;
			int warning = 0;
			my_timeval	tm{0,0};
			if (!datetime_with_no_zero_in_date_to_timeval(&t, *thd->time_zone(), &tm, &warning) || (warning & MYSQL_TIME_WARN_TRUNCATED)) {
				spw_print_warning("Baddly formatted timestamp type in WHERE clause: %s. Sparrow can't use partition pruning.", thd->query().str);
				return -1;
			} else if (warning != 0) {
				spw_print_warning("Baddly formatted timestamp type in WHERE clause: %s: warning %d", thd->query().str, warning);
			}
			return tm.m_tv_sec * 1000ULL + tm.m_tv_usec/1000;		// return timestamp in ms
		}
	} else {
		[[maybe_unused]] THD* thd = current_thd;
		spw_print_warning("Wrong timestamp type in WHERE clause: %s. Sparrow can't use partition pruning.", thd->query().str);
	}
	
	return -1;
}

}
