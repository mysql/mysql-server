/*
   Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
  This file defines the NDB Cluster handler engine_condition_pushdown
*/

#include "ha_ndbcluster_glue.h"
#include <ndbapi/NdbApi.hpp>
#include "ha_ndbcluster_cond.h"

// Typedefs for long names 
typedef NdbDictionary::Column NDBCOL;
typedef NdbDictionary::Table NDBTAB;

/*
  Serialize the item tree into a linked list represented by Ndb_cond
  for fast generation of NbdScanFilter. Adds information such as
  position of fields that is not directly available in the Item tree.
  Also checks if condition is supported.
*/
static void
ndb_serialize_cond(const Item *item, void *arg)
{
  Ndb_cond_traverse_context *context= (Ndb_cond_traverse_context *) arg;
  DBUG_ENTER("ndb_serialize_cond");  

  // Check if we are skipping arguments to a function to be evaluated
  if (context->skip)
  {
    if (!item)
    {
      DBUG_PRINT("info", ("Unexpected mismatch of found and expected number of function arguments %u", context->skip));
      sql_print_error("ndb_serialize_cond: Unexpected mismatch of found and "
                      "expected number of function arguments %u", context->skip);
      context->skip= 0;
      DBUG_VOID_RETURN;
    }
    DBUG_PRINT("info", ("Skiping argument %d", context->skip));
    context->skip--;
    switch (item->type()) {
    case Item::FUNC_ITEM:
    {
      Item_func *func_item= (Item_func *) item;
      context->skip+= func_item->argument_count();
      break;
    }
    case Item::INT_ITEM:
    case Item::REAL_ITEM:
    case Item::STRING_ITEM:
    case Item::VARBIN_ITEM:
    case Item::DECIMAL_ITEM:
      break;
    default:
      context->supported= FALSE;
      break;
    }
    
    DBUG_VOID_RETURN;
  }
  
  if (context->supported)
  {
    Ndb_rewrite_context *rewrite_context2= context->rewrite_stack;
    const Item_func *rewrite_func_item;
    // Check if we are rewriting some unsupported function call
    if (rewrite_context2 &&
        (rewrite_func_item= rewrite_context2->func_item) &&
        rewrite_context2->count++ == 0)
    {
      switch (rewrite_func_item->functype()) {
      case Item_func::BETWEEN:
        /*
          Rewrite 
          <field>|<const> BETWEEN <const1>|<field1> AND <const2>|<field2>
          to <field>|<const> > <const1>|<field1> AND 
          <field>|<const> < <const2>|<field2>
          or actually in prefix format
          BEGIN(AND) GT(<field>|<const>, <const1>|<field1>), 
          LT(<field>|<const>, <const2>|<field2>), END()
        */
      case Item_func::IN_FUNC:
      {
        /*
          Rewrite <field>|<const> IN(<const1>|<field1>, <const2>|<field2>,..)
          to <field>|<const> = <const1>|<field1> OR 
          <field> = <const2>|<field2> ...
          or actually in prefix format
          BEGIN(OR) EQ(<field>|<const>, <const1><field1>), 
          EQ(<field>|<const>, <const2>|<field2>), ... END()
          Each part of the disjunction is added for each call
          to ndb_serialize_cond and end of rewrite statement 
          is wrapped in end of ndb_serialize_cond
        */
        if (context->expecting(item->type()))
        {
          // This is the <field>|<const> item, save it in the rewrite context
          rewrite_context2->left_hand_item= item;
          if (item->type() == Item::FUNC_ITEM)
          {
            Item_func *func_item= (Item_func *) item;
            if ((func_item->functype() == Item_func::UNKNOWN_FUNC ||
                 func_item->functype() == Item_func::NEG_FUNC) &&
                func_item->const_item())
            {
              // Skip any arguments since we will evaluate function instead
              DBUG_PRINT("info", ("Skip until end of arguments marker"));
              context->skip= func_item->argument_count();
            }
            else
            {
              DBUG_PRINT("info", ("Found unsupported functional expression in BETWEEN|IN"));
              context->supported= FALSE;
              DBUG_VOID_RETURN;
              
            }
          }
        }
        else
        {
          // Non-supported BETWEEN|IN expression
          DBUG_PRINT("info", ("Found unexpected item of type %u in BETWEEN|IN",
                              item->type()));
          context->supported= FALSE;
          DBUG_VOID_RETURN;
        }
        break;
      }
      default:
        context->supported= FALSE;
        break;
      }
      DBUG_VOID_RETURN;
    }
    else
    {
      Ndb_cond_stack *ndb_stack= context->cond_stack;
      Ndb_cond *prev_cond= context->cond_ptr;
      Ndb_cond *curr_cond= context->cond_ptr= new Ndb_cond();
      if (!ndb_stack->ndb_cond)
        ndb_stack->ndb_cond= curr_cond;
      curr_cond->prev= prev_cond;
      if (prev_cond) prev_cond->next= curr_cond;
    // Check if we are rewriting some unsupported function call
      if (context->rewrite_stack)
      {
        Ndb_rewrite_context *rewrite_context= context->rewrite_stack;
        const Item_func *func_item= rewrite_context->func_item;
        switch (func_item->functype()) {
        case Item_func::BETWEEN:
        {
          /*
            Rewrite 
            <field>|<const> BETWEEN <const1>|<field1> AND <const2>|<field2>
            to <field>|<const> > <const1>|<field1> AND 
            <field>|<const> < <const2>|<field2>
            or actually in prefix format
            BEGIN(AND) GT(<field>|<const>, <const1>|<field1>), 
            LT(<field>|<const>, <const2>|<field2>), END()
          */
          if (rewrite_context->count == 2)
          {
            // Lower limit of BETWEEN
            DBUG_PRINT("info", ("GE_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(Item_func::GE_FUNC, 2);
          }
          else if (rewrite_context->count == 3)
          {
            // Upper limit of BETWEEN
            DBUG_PRINT("info", ("LE_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(Item_func::LE_FUNC, 2);
          }
          else
          {
            // Illegal BETWEEN expression
            DBUG_PRINT("info", ("Illegal BETWEEN expression"));
            context->supported= FALSE;
            DBUG_VOID_RETURN;
          }
          break;
        }
        case Item_func::IN_FUNC:
        {
          /*
            Rewrite <field>|<const> IN(<const1>|<field1>, <const2>|<field2>,..)
            to <field>|<const> = <const1>|<field1> OR 
            <field> = <const2>|<field2> ...
            or actually in prefix format
            BEGIN(OR) EQ(<field>|<const>, <const1><field1>), 
            EQ(<field>|<const>, <const2>|<field2>), ... END()
            Each part of the disjunction is added for each call
            to ndb_serialize_cond and end of rewrite statement 
            is wrapped in end of ndb_serialize_cond
          */
          DBUG_PRINT("info", ("EQ_FUNC"));      
          curr_cond->ndb_item= new Ndb_item(Item_func::EQ_FUNC, 2);
          break;
        }
        default:
          context->supported= FALSE;
        }
        // Handle left hand <field>|<const>
        context->rewrite_stack= NULL; // Disable rewrite mode
        context->expect_only(Item::FIELD_ITEM);
        context->expect_field_result(STRING_RESULT);
        context->expect_field_result(REAL_RESULT);
        context->expect_field_result(INT_RESULT);
        context->expect_field_result(DECIMAL_RESULT);
        context->expect(Item::INT_ITEM);
        context->expect(Item::STRING_ITEM);
        context->expect(Item::VARBIN_ITEM);
        context->expect(Item::FUNC_ITEM);
        context->expect(Item::CACHE_ITEM);
        ndb_serialize_cond(rewrite_context->left_hand_item, arg);
        context->skip= 0; // Any FUNC_ITEM expression has already been parsed
        context->rewrite_stack= rewrite_context; // Enable rewrite mode
        if (!context->supported)
          DBUG_VOID_RETURN;

        prev_cond= context->cond_ptr;
        curr_cond= context->cond_ptr= new Ndb_cond();
        prev_cond->next= curr_cond;
      }
      
      // Check for end of AND/OR expression
      if (!item)
      {
        // End marker for condition group
        DBUG_PRINT("info", ("End of condition group"));
        context->expect_no_length();
        curr_cond->ndb_item= new Ndb_item(NDB_END_COND);
      }
      else
      {
        bool pop= TRUE;

        switch (item->type()) {
        case Item::FIELD_ITEM:
        {
          Item_field *field_item= (Item_field *) item;
          Field *field= field_item->field;
          const enum_field_types type= field->real_type();
          /*
            Check that the field is part of the table of the handler
            instance and that we expect a field with of this result type.
          */
          if (context->table->s == field->table->s)
          {       
            const NDBTAB *tab= context->ndb_table;
            DBUG_PRINT("info", ("FIELD_ITEM"));
            DBUG_PRINT("info", ("table %s", tab->getName()));
            DBUG_PRINT("info", ("column %s", field->field_name));
            DBUG_PRINT("info", ("column length %u", field->field_length));
            DBUG_PRINT("info", ("type %d", type));
            DBUG_PRINT("info", ("result type %d", field->result_type()));

            // Check that we are expecting a field and with the correct
            // result type and of length that can store the item value
            if (context->expecting(Item::FIELD_ITEM) &&
                context->expecting_field_type(type) &&
                context->expecting_max_length(field->field_length) &&
                (context->expecting_field_result(field->result_type()) ||
                 // Date and year can be written as string or int
                 ((type == MYSQL_TYPE_TIME ||
                   type == MYSQL_TYPE_TIME2 ||
                   type == MYSQL_TYPE_DATE || 
                   type == MYSQL_TYPE_NEWDATE || 
                   type == MYSQL_TYPE_YEAR ||
                   type == MYSQL_TYPE_DATETIME ||
                   type == MYSQL_TYPE_DATETIME2)
                  ? (context->expecting_field_result(STRING_RESULT) ||
                     context->expecting_field_result(INT_RESULT))
                  : TRUE)) &&
                // Bit fields no yet supported in scan filter
                type != MYSQL_TYPE_BIT &&
                /* Char(0) field is treated as Bit fields inside NDB
                   Hence not supported in scan filter */
                (!(type == MYSQL_TYPE_STRING && field->pack_length() == 0)) &&
                // No BLOB support in scan filter
                type != MYSQL_TYPE_TINY_BLOB &&
                type != MYSQL_TYPE_MEDIUM_BLOB &&
                type != MYSQL_TYPE_LONG_BLOB &&
                type != MYSQL_TYPE_BLOB &&
                type != MYSQL_TYPE_JSON &&
                type != MYSQL_TYPE_GEOMETRY)
            {
              const NDBCOL *col= tab->getColumn(field->field_name);
              DBUG_ASSERT(col);
              curr_cond->ndb_item= new Ndb_item(field, col->getColumnNo());
              context->dont_expect(Item::FIELD_ITEM);
              context->expect_no_field_result();
              if (! context->expecting_nothing())
              {
                // We have not seen second argument yet
                if (type == MYSQL_TYPE_TIME ||
                    type == MYSQL_TYPE_TIME2 ||
                    type == MYSQL_TYPE_DATE || 
                    type == MYSQL_TYPE_NEWDATE || 
                    type == MYSQL_TYPE_YEAR ||
                    type == MYSQL_TYPE_DATETIME ||
                    type == MYSQL_TYPE_DATETIME2)
                {
                  context->expect_only(Item::STRING_ITEM);
                  context->expect(Item::INT_ITEM);
                }
                else
                  switch (field->result_type()) {
                  case STRING_RESULT:
                    // Expect char string or binary string
                    context->expect_only(Item::STRING_ITEM);
                    context->expect(Item::VARBIN_ITEM);
                    context->expect_collation(field_item->collation.collation);
                    context->expect_max_length(field->field_length);
                    break;
                  case REAL_RESULT:
                    context->expect_only(Item::REAL_ITEM);
                    context->expect(Item::DECIMAL_ITEM);
                    context->expect(Item::INT_ITEM);
                    break;
                  case INT_RESULT:
                    context->expect_only(Item::INT_ITEM);
                    context->expect(Item::VARBIN_ITEM);
                    break;
                  case DECIMAL_RESULT:
                    context->expect_only(Item::DECIMAL_ITEM);
                    context->expect(Item::REAL_ITEM);
                    context->expect(Item::INT_ITEM);
                    break;
                  default:
                    break;
                  }    
              }
              else
              {
                // Expect another logical expression
                context->expect_only(Item::FUNC_ITEM);
                context->expect(Item::COND_ITEM);
                // Check that field and string constant collations are the same
                if ((field->result_type() == STRING_RESULT) &&
                    !context->expecting_collation(item->collation.collation)
                    && type != MYSQL_TYPE_TIME
                    && type != MYSQL_TYPE_TIME2
                    && type != MYSQL_TYPE_DATE
                    && type != MYSQL_TYPE_NEWDATE
                    && type != MYSQL_TYPE_YEAR
                    && type != MYSQL_TYPE_DATETIME
                    && type != MYSQL_TYPE_DATETIME2)
                {
                  DBUG_PRINT("info", ("Found non-matching collation %s",  
                                      item->collation.collation->name)); 
                  context->supported= FALSE;                
                }
              }
              break;
            }
            else
            {
              DBUG_PRINT("info", ("Was not expecting field of type %u(%u)",
                                  field->result_type(), type));
              context->supported= FALSE;
            }
          }
          else
          {
            DBUG_PRINT("info", ("Was not expecting field from table %s (%s)",
                                context->table->s->table_name.str, 
                                field->table->s->table_name.str));
            context->supported= FALSE;
          }
          break;
        }
        case Item::FUNC_ITEM:
        {
          Item_func *func_item= (Item_func *) item;
          // Check that we expect a function or functional expression here
          if (context->expecting(Item::FUNC_ITEM) ||
              func_item->functype() == Item_func::UNKNOWN_FUNC ||
              func_item->functype() == Item_func::NEG_FUNC)
            context->expect_nothing();
          else
          {
            // Did not expect function here
            context->supported= FALSE;
            break;
          }
          context->expect_no_length();
          switch (func_item->functype()) {
          case Item_func::EQ_FUNC:
          {
            DBUG_PRINT("info", ("EQ_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(func_item->functype(), 
                                              func_item);      
            context->expect(Item::STRING_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::REAL_ITEM);
            context->expect(Item::DECIMAL_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect(Item::FIELD_ITEM);
            context->expect_field_result(STRING_RESULT);
            context->expect_field_result(REAL_RESULT);
            context->expect_field_result(INT_RESULT);
            context->expect_field_result(DECIMAL_RESULT);
            break;
          }
          case Item_func::NE_FUNC:
          {
            DBUG_PRINT("info", ("NE_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(func_item->functype(),
                                              func_item);      
            context->expect(Item::STRING_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::REAL_ITEM);
            context->expect(Item::DECIMAL_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect(Item::FIELD_ITEM);
            context->expect_field_result(STRING_RESULT);
            context->expect_field_result(REAL_RESULT);
            context->expect_field_result(INT_RESULT);
            context->expect_field_result(DECIMAL_RESULT);
            break;
          }
          case Item_func::LT_FUNC:
          {
            DBUG_PRINT("info", ("LT_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(func_item->functype(),
                                              func_item);      
            context->expect(Item::STRING_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::REAL_ITEM);
            context->expect(Item::DECIMAL_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect(Item::FIELD_ITEM);
            context->expect_field_result(STRING_RESULT);
            context->expect_field_result(REAL_RESULT);
            context->expect_field_result(INT_RESULT);
            context->expect_field_result(DECIMAL_RESULT);
            break;
          }
          case Item_func::LE_FUNC:
          {
            DBUG_PRINT("info", ("LE_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(func_item->functype(),
                                              func_item);      
            context->expect(Item::STRING_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::REAL_ITEM);
            context->expect(Item::DECIMAL_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect(Item::FIELD_ITEM);
            context->expect_field_result(STRING_RESULT);
            context->expect_field_result(REAL_RESULT);
            context->expect_field_result(INT_RESULT);
            context->expect_field_result(DECIMAL_RESULT);
            break;
          }
          case Item_func::GE_FUNC:
          {
            DBUG_PRINT("info", ("GE_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(func_item->functype(),
                                              func_item);      
            context->expect(Item::STRING_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::REAL_ITEM);
            context->expect(Item::DECIMAL_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect(Item::FIELD_ITEM);
            context->expect_field_result(STRING_RESULT);
            context->expect_field_result(REAL_RESULT);
            context->expect_field_result(INT_RESULT);
            context->expect_field_result(DECIMAL_RESULT);
            break;
          }
          case Item_func::GT_FUNC:
          {
            DBUG_PRINT("info", ("GT_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(func_item->functype(),
                                              func_item);      
            context->expect(Item::STRING_ITEM);
            context->expect(Item::REAL_ITEM);
            context->expect(Item::DECIMAL_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect(Item::FIELD_ITEM);
            context->expect_field_result(STRING_RESULT);
            context->expect_field_result(REAL_RESULT);
            context->expect_field_result(INT_RESULT);
            context->expect_field_result(DECIMAL_RESULT);
            break;
          }
          case Item_func::LIKE_FUNC:
          {
            Ndb_expect_stack* expect_next= new Ndb_expect_stack();
            DBUG_PRINT("info", ("LIKE_FUNC"));

            if (((const Item_func_like *)func_item)->escape_was_used_in_parsing())
            {
              DBUG_PRINT("info", ("LIKE expressions with ESCAPE not supported"));
              context->supported= FALSE;
            }
            curr_cond->ndb_item= new Ndb_item(func_item->functype(),
                                              func_item);

            /*
              Ndb currently only supports pushing
              <field> LIKE <string> | <func>
              we thus push <string> | <func>
              on the expect stack to catch that we
              don't support <string> LIKE <field>.
             */
            context->expect(Item::FIELD_ITEM);
            context->expect_only_field_type(MYSQL_TYPE_STRING);
            context->expect_field_type(MYSQL_TYPE_VAR_STRING);
            context->expect_field_type(MYSQL_TYPE_VARCHAR);
            context->expect_field_result(STRING_RESULT);
            expect_next->expect(Item::STRING_ITEM);
            expect_next->expect(Item::FUNC_ITEM);
            context->expect_stack.push(expect_next);
            pop= FALSE;
            break;
          }
          case Item_func::ISNULL_FUNC:
          {
            DBUG_PRINT("info", ("ISNULL_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(func_item->functype(),
                                              func_item);      
            context->expect(Item::FIELD_ITEM);
            context->expect_field_result(STRING_RESULT);
            context->expect_field_result(REAL_RESULT);
            context->expect_field_result(INT_RESULT);
            context->expect_field_result(DECIMAL_RESULT);
            break;
          }
          case Item_func::ISNOTNULL_FUNC:
          {
            DBUG_PRINT("info", ("ISNOTNULL_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(func_item->functype(),
                                              func_item);     
            context->expect(Item::FIELD_ITEM);
            context->expect_field_result(STRING_RESULT);
            context->expect_field_result(REAL_RESULT);
            context->expect_field_result(INT_RESULT);
            context->expect_field_result(DECIMAL_RESULT);
            break;
          }
          case Item_func::NOT_FUNC:
          {
            DBUG_PRINT("info", ("NOT_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(func_item->functype(),
                                              func_item);     
            context->expect(Item::FUNC_ITEM);
            context->expect(Item::COND_ITEM);
            break;
          }
          case Item_func::BETWEEN:
          {
            DBUG_PRINT("info", ("BETWEEN, rewriting using AND"));
            Item_func_between *between_func= (Item_func_between *) func_item;
            Ndb_rewrite_context *rewrite_context= 
              new Ndb_rewrite_context(func_item);
            rewrite_context->next= context->rewrite_stack;
            context->rewrite_stack= rewrite_context;
            if (between_func->negated)
            {
              DBUG_PRINT("info", ("NOT_FUNC"));
              curr_cond->ndb_item= new Ndb_item(Item_func::NOT_FUNC, 1);
              prev_cond= curr_cond;
              curr_cond= context->cond_ptr= new Ndb_cond();
              curr_cond->prev= prev_cond;
              prev_cond->next= curr_cond;
            }
            DBUG_PRINT("info", ("COND_AND_FUNC"));
            curr_cond->ndb_item= 
              new Ndb_item(Item_func::COND_AND_FUNC, 
                           func_item->argument_count() - 1);
            context->expect_only(Item::FIELD_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::STRING_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect(Item::FUNC_ITEM);
            context->expect(Item::CACHE_ITEM);
            break;
          }
          case Item_func::IN_FUNC:
          {
            DBUG_PRINT("info", ("IN_FUNC, rewriting using OR"));
            Item_func_in *in_func= (Item_func_in *) func_item;
            Ndb_rewrite_context *rewrite_context= 
              new Ndb_rewrite_context(func_item);
            rewrite_context->next= context->rewrite_stack;
            context->rewrite_stack= rewrite_context;
            if (in_func->negated)
            {
              DBUG_PRINT("info", ("NOT_FUNC"));
              curr_cond->ndb_item= new Ndb_item(Item_func::NOT_FUNC, 1);
              prev_cond= curr_cond;
              curr_cond= context->cond_ptr= new Ndb_cond();
              curr_cond->prev= prev_cond;
              prev_cond->next= curr_cond;
            }
            DBUG_PRINT("info", ("COND_OR_FUNC"));
            curr_cond->ndb_item= new Ndb_item(Item_func::COND_OR_FUNC, 
                                              func_item->argument_count() - 1);
            context->expect_only(Item::FIELD_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::STRING_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect(Item::FUNC_ITEM);
            context->expect(Item::CACHE_ITEM);
            break;
          }
          case Item_func::NEG_FUNC:
          case Item_func::UNKNOWN_FUNC:
          {
            /*
              Constant expressions of the type
              -17, 1+2, concat(0xBB, '%') will
              be evaluated before pushed.
             */
            DBUG_PRINT("info", ("Function %s", 
                                func_item->const_item()?"const":""));  
            DBUG_PRINT("info", ("result type %d", func_item->result_type()));
            /*
              Check if we are rewriting queries of the type
              <const> BETWEEN|IN <func> ...
              as this is currently not supported.
             */
            if (context->rewrite_stack &&
                context->rewrite_stack->left_hand_item &&
                context->rewrite_stack->left_hand_item->type()
                != Item::FIELD_ITEM)
            {
              DBUG_PRINT("info", ("Function during rewrite not supported"));
              context->supported= FALSE;
            }
            if (func_item->const_item())
            {
              switch (func_item->result_type()) {
              case STRING_RESULT:
              {
                NDB_ITEM_QUALIFICATION q;
                q.value_type= Item::STRING_ITEM;
                curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item); 
                if (! context->expecting_no_field_result())
                {
                  // We have not seen the field argument yet
                  context->expect_only(Item::FIELD_ITEM);
                  context->expect_only_field_result(STRING_RESULT);
                  context->expect_collation(func_item->collation.collation);
                }
                else
                {
                  // Expect another logical expression
                  context->expect_only(Item::FUNC_ITEM);
                  context->expect(Item::COND_ITEM);
                  // Check that string result have correct collation
                  if (!context->expecting_collation(item->collation.collation))
                  {
                    DBUG_PRINT("info", ("Found non-matching collation %s",  
                                        item->collation.collation->name));
                    context->supported= FALSE;
                  }
                }
                // Skip any arguments since we will evaluate function instead
                DBUG_PRINT("info", ("Skip until end of arguments marker"));
                context->skip= func_item->argument_count();
                break;
              }
              case REAL_RESULT:
              {
                NDB_ITEM_QUALIFICATION q;
                q.value_type= Item::REAL_ITEM;
                curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);
                if (! context->expecting_no_field_result()) 
                {
                  // We have not seen the field argument yet
                  context->expect_only(Item::FIELD_ITEM);
                  context->expect_only_field_result(REAL_RESULT);
                }
                else
                {
                  // Expect another logical expression
                  context->expect_only(Item::FUNC_ITEM);
                  context->expect(Item::COND_ITEM);
                }
                
                // Skip any arguments since we will evaluate function instead
                DBUG_PRINT("info", ("Skip until end of arguments marker"));
                context->skip= func_item->argument_count();
                break;
              }
              case INT_RESULT:
              {
                NDB_ITEM_QUALIFICATION q;
                q.value_type= Item::INT_ITEM;
                curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);
                if (! context->expecting_no_field_result()) 
                {
                  // We have not seen the field argument yet
                  context->expect_only(Item::FIELD_ITEM);
                  context->expect_only_field_result(INT_RESULT);
                }
                else
                {
                  // Expect another logical expression
                  context->expect_only(Item::FUNC_ITEM);
                  context->expect(Item::COND_ITEM);
                }
                
                // Skip any arguments since we will evaluate function instead
                DBUG_PRINT("info", ("Skip until end of arguments marker"));
                context->skip= func_item->argument_count();
                break;
              }
              case DECIMAL_RESULT:
              {
                NDB_ITEM_QUALIFICATION q;
                q.value_type= Item::DECIMAL_ITEM;
                curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);
                if (! context->expecting_no_field_result()) 
                {
                  // We have not seen the field argument yet
                  context->expect_only(Item::FIELD_ITEM);
                  context->expect_only_field_result(DECIMAL_RESULT);
                }
                else
                {
                  // Expect another logical expression
                  context->expect_only(Item::FUNC_ITEM);
                  context->expect(Item::COND_ITEM);
                }
                // Skip any arguments since we will evaluate function instead
                DBUG_PRINT("info", ("Skip until end of arguments marker"));
                context->skip= func_item->argument_count();
                break;
              }
              default:
                break;
              }
            }
            else
              // Function does not return constant expression
              context->supported= FALSE;
            break;
          }
          default:
          {
            DBUG_PRINT("info", ("Found func_item of type %d", 
                                func_item->functype()));
            context->supported= FALSE;
          }
          }
          break;
        }
        case Item::STRING_ITEM:
          DBUG_PRINT("info", ("STRING_ITEM")); 
          // Check that we do support pushing the item value length
          if (context->expecting(Item::STRING_ITEM) &&
              context->expecting_length(item->max_length)) 
          {
#ifndef DBUG_OFF
            char buff[256];
            String str(buff, 0, system_charset_info);
            const_cast<Item*>(item)->print(&str, QT_ORDINARY);
            DBUG_PRINT("info", ("value: '%s'", str.c_ptr_safe()));
#endif
            NDB_ITEM_QUALIFICATION q;
            q.value_type= Item::STRING_ITEM;
            curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);      
            if (! context->expecting_no_field_result())
            {
              // We have not seen the field argument yet
              context->expect_only(Item::FIELD_ITEM);
              context->expect_only_field_result(STRING_RESULT);
              context->expect_collation(item->collation.collation);
              context->expect_length(item->max_length);
            }
            else 
            {
              // Expect another logical expression
              context->expect_only(Item::FUNC_ITEM);
              context->expect(Item::COND_ITEM);
              context->expect_no_length();
              // Check that we are comparing with a field with same collation
              if (!context->expecting_collation(item->collation.collation))
              {
                DBUG_PRINT("info", ("Found non-matching collation %s",  
                                    item->collation.collation->name));
                context->supported= FALSE;
              }
            }
          }
          else
            context->supported= FALSE;
          break;
        case Item::INT_ITEM:
          DBUG_PRINT("info", ("INT_ITEM"));
          if (context->expecting(Item::INT_ITEM)) 
          {
            DBUG_PRINT("info", ("value %ld",
                                (long) ((Item_int*) item)->value));
            NDB_ITEM_QUALIFICATION q;
            q.value_type= Item::INT_ITEM;
            curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);
            if (! context->expecting_no_field_result()) 
            {
              // We have not seen the field argument yet
              context->expect_only(Item::FIELD_ITEM);
              context->expect_only_field_result(INT_RESULT);
              context->expect_field_result(REAL_RESULT);
              context->expect_field_result(DECIMAL_RESULT);
            }
            else
            {
              // Expect another logical expression
              context->expect_only(Item::FUNC_ITEM);
              context->expect(Item::COND_ITEM);
            }
          }
          else
            context->supported= FALSE;
          break;
        case Item::REAL_ITEM:
          DBUG_PRINT("info", ("REAL_ITEM"));
          if (context->expecting(Item::REAL_ITEM)) 
          {
            DBUG_PRINT("info", ("value %f", ((Item_float*) item)->value));
            NDB_ITEM_QUALIFICATION q;
            q.value_type= Item::REAL_ITEM;
            curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);
            if (! context->expecting_no_field_result()) 
            {
              // We have not seen the field argument yet
              context->expect_only(Item::FIELD_ITEM);
              context->expect_only_field_result(REAL_RESULT);
            }
            else
            {
              // Expect another logical expression
              context->expect_only(Item::FUNC_ITEM);
              context->expect(Item::COND_ITEM);
            }
          }
          else
            context->supported= FALSE;
          break;
        case Item::VARBIN_ITEM:
          DBUG_PRINT("info", ("VARBIN_ITEM"));
          if (context->expecting(Item::VARBIN_ITEM)) 
          {
            NDB_ITEM_QUALIFICATION q;
            q.value_type= Item::VARBIN_ITEM;
            curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);      
            if (! context->expecting_no_field_result())
            {
              // We have not seen the field argument yet
              context->expect_only(Item::FIELD_ITEM);
              context->expect_only_field_result(STRING_RESULT);
            }
            else
            {
              // Expect another logical expression
              context->expect_only(Item::FUNC_ITEM);
              context->expect(Item::COND_ITEM);
            }
          }
          else
            context->supported= FALSE;
          break;
        case Item::DECIMAL_ITEM:
          DBUG_PRINT("info", ("DECIMAL_ITEM"));
          if (context->expecting(Item::DECIMAL_ITEM)) 
          {
            DBUG_PRINT("info", ("value %f",
                                ((Item_decimal*) item)->val_real()));
            NDB_ITEM_QUALIFICATION q;
            q.value_type= Item::DECIMAL_ITEM;
            curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);
            if (! context->expecting_no_field_result()) 
            {
              // We have not seen the field argument yet
              context->expect_only(Item::FIELD_ITEM);
              context->expect_only_field_result(REAL_RESULT);
              context->expect_field_result(DECIMAL_RESULT);
            }
            else
            {
              // Expect another logical expression
              context->expect_only(Item::FUNC_ITEM);
              context->expect(Item::COND_ITEM);
            }
          }
          else
            context->supported= FALSE;
          break;
        case Item::COND_ITEM:
        {
          Item_cond *cond_item= (Item_cond *) item;
          
          if (context->expecting(Item::COND_ITEM))
          {
            switch (cond_item->functype()) {
            case Item_func::COND_AND_FUNC:
              DBUG_PRINT("info", ("COND_AND_FUNC"));
              curr_cond->ndb_item= new Ndb_item(cond_item->functype(),
                                                cond_item);      
              break;
            case Item_func::COND_OR_FUNC:
              DBUG_PRINT("info", ("COND_OR_FUNC"));
              curr_cond->ndb_item= new Ndb_item(cond_item->functype(),
                                                cond_item);      
              break;
            default:
              DBUG_PRINT("info", ("COND_ITEM %d", cond_item->functype()));
              context->supported= FALSE;
              break;
            }
          }
          else
          {
            /* Did not expect condition */
            context->supported= FALSE;          
          }
          break;
        }
        case Item::CACHE_ITEM:
        {
          DBUG_PRINT("info", ("CACHE_ITEM"));
          Item_cache* cache_item = (Item_cache*)item;
          DBUG_PRINT("info", ("result type %d", cache_item->result_type()));

          // Item_cache has cached "something", use its value
          // based on the result_type of the item
          switch(cache_item->result_type())
          {
          case INT_RESULT:
            DBUG_PRINT("info", ("INT_RESULT"));
            if (context->expecting(Item::INT_ITEM)) 
            {
              DBUG_PRINT("info", ("value %ld",
                                  (long) ((Item_int*) item)->value));
              NDB_ITEM_QUALIFICATION q;
              q.value_type= Item::INT_ITEM;
              curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);
              if (! context->expecting_no_field_result()) 
              {
                // We have not seen the field argument yet
                context->expect_only(Item::FIELD_ITEM);
                context->expect_only_field_result(INT_RESULT);
                context->expect_field_result(REAL_RESULT);
                context->expect_field_result(DECIMAL_RESULT);
              }
              else
              {
                // Expect another logical expression
                context->expect_only(Item::FUNC_ITEM);
                context->expect(Item::COND_ITEM);
              }
            }
            else
              context->supported= FALSE;
            break;

          case REAL_RESULT:
            DBUG_PRINT("info", ("REAL_RESULT"));
            if (context->expecting(Item::REAL_ITEM)) 
            {
              DBUG_PRINT("info", ("value %f", ((Item_float*) item)->value));
              NDB_ITEM_QUALIFICATION q;
              q.value_type= Item::REAL_ITEM;
              curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);
              if (! context->expecting_no_field_result()) 
              {
                // We have not seen the field argument yet
                context->expect_only(Item::FIELD_ITEM);
                context->expect_only_field_result(REAL_RESULT);
              }
              else
              {
                // Expect another logical expression
                context->expect_only(Item::FUNC_ITEM);
                context->expect(Item::COND_ITEM);
              }
            }
            else
              context->supported= FALSE;
            break;

          case DECIMAL_RESULT:
            DBUG_PRINT("info", ("DECIMAL_RESULT"));
            if (context->expecting(Item::DECIMAL_ITEM)) 
            {
              DBUG_PRINT("info", ("value %f",
                                  ((Item_decimal*) item)->val_real()));
              NDB_ITEM_QUALIFICATION q;
              q.value_type= Item::DECIMAL_ITEM;
              curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);
              if (! context->expecting_no_field_result()) 
              {
                // We have not seen the field argument yet
                context->expect_only(Item::FIELD_ITEM);
                context->expect_only_field_result(REAL_RESULT);
                context->expect_field_result(DECIMAL_RESULT);
              }
              else
              {
                // Expect another logical expression
                context->expect_only(Item::FUNC_ITEM);
                context->expect(Item::COND_ITEM);
              }
            }
            else
              context->supported= FALSE;
            break;

          case STRING_RESULT:
            DBUG_PRINT("info", ("STRING_RESULT")); 
            // Check that we do support pushing the item value length
            if (context->expecting(Item::STRING_ITEM) &&
                context->expecting_length(item->max_length)) 
            {
  #ifndef DBUG_OFF
              char buff[256];
              String str(buff, 0, system_charset_info);
              const_cast<Item*>(item)->print(&str, QT_ORDINARY);
              DBUG_PRINT("info", ("value: '%s'", str.c_ptr_safe()));
  #endif
              NDB_ITEM_QUALIFICATION q;
              q.value_type= Item::STRING_ITEM;
              curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);      
              if (! context->expecting_no_field_result())
              {
                // We have not seen the field argument yet
                context->expect_only(Item::FIELD_ITEM);
                context->expect_only_field_result(STRING_RESULT);
                context->expect_collation(item->collation.collation);
                context->expect_length(item->max_length);
              }
              else 
              {
                // Expect another logical expression
                context->expect_only(Item::FUNC_ITEM);
                context->expect(Item::COND_ITEM);
                context->expect_no_length();
                // Check that we are comparing with a field with same collation
                if (!context->expecting_collation(item->collation.collation))
                {
                  DBUG_PRINT("info", ("Found non-matching collation %s",  
                                      item->collation.collation->name));
                  context->supported= FALSE;
                }
              }
            }
            else
              context->supported= FALSE;
            break;

          default:
            context->supported= FALSE;
            break;
          }
          break;
        }

        default:
        {
          DBUG_PRINT("info", ("Found unsupported item of type %d",
                              item->type()));
          context->supported= FALSE;
        }
        }
        if (pop)
          context->expect_stack.pop();
      }
      if (context->supported && context->rewrite_stack)
      {
        Ndb_rewrite_context *rewrite_context= context->rewrite_stack;
        if (rewrite_context->count == 
            rewrite_context->func_item->argument_count())
        {
          // Rewrite is done, wrap an END() at the en
          DBUG_PRINT("info", ("End of condition group"));
          prev_cond= curr_cond;
          curr_cond= context->cond_ptr= new Ndb_cond();
          curr_cond->prev= prev_cond;
          prev_cond->next= curr_cond;
          context->expect_no_length();
          curr_cond->ndb_item= new Ndb_item(NDB_END_COND);
          // Pop rewrite stack
          context->rewrite_stack=  rewrite_context->next;
          rewrite_context->next= NULL;
          delete(rewrite_context);
        }
      }
    }
  }
 
  DBUG_VOID_RETURN;
}

/*
  Push a condition
 */
const 
Item* 
ha_ndbcluster_cond::cond_push(const Item *cond, 
                              TABLE *table, const NDBTAB *ndb_table)
{ 
  DBUG_ENTER("ha_ndbcluster_cond::cond_push");
  Ndb_cond_stack *ndb_cond = new Ndb_cond_stack();
  if (ndb_cond == NULL)
  {
    set_my_errno(HA_ERR_OUT_OF_MEM);
    DBUG_RETURN(cond);
  }
  if (m_cond_stack)
    ndb_cond->next= m_cond_stack;
  else
    ndb_cond->next= NULL;
  m_cond_stack= ndb_cond;
  
  if (serialize_cond(cond, ndb_cond, table, ndb_table))
  {
    DBUG_RETURN(NULL);
  }
  else
  {
    cond_pop();
  }
  DBUG_RETURN(cond); 
}

/*
  Pop the top condition from the condition stack
*/
void 
ha_ndbcluster_cond::cond_pop() 
{ 
  Ndb_cond_stack *ndb_cond_stack= m_cond_stack;  
  if (ndb_cond_stack)
  {
    m_cond_stack= ndb_cond_stack->next;
    ndb_cond_stack->next= NULL;
    delete ndb_cond_stack;
  }
}

/*
  Clear the condition stack
*/
void
ha_ndbcluster_cond::cond_clear()
{
  DBUG_ENTER("cond_clear");
  while (m_cond_stack)
    cond_pop();

  DBUG_VOID_RETURN;
}

bool
ha_ndbcluster_cond::serialize_cond(const Item *cond, Ndb_cond_stack *ndb_cond,
                                   TABLE *table,
                                   const NDBTAB *ndb_table) const
{
  DBUG_ENTER("serialize_cond");
  Item *item= (Item *) cond;
  Ndb_cond_traverse_context context(table, ndb_table, ndb_cond);
  // Expect a logical expression
  context.expect(Item::FUNC_ITEM);
  context.expect(Item::COND_ITEM);
  item->traverse_cond(&ndb_serialize_cond, (void *) &context, Item::PREFIX);
  DBUG_PRINT("info", ("The pushed condition is %ssupported", (context.supported)?"":"not "));

  DBUG_RETURN(context.supported);
}

int
ha_ndbcluster_cond::build_scan_filter_predicate(Ndb_cond * &cond, 
                                                NdbScanFilter *filter,
                                                bool negated) const
{
  DBUG_ENTER("build_scan_filter_predicate");  
  switch (cond->ndb_item->type) {
  case NDB_FUNCTION:
  {
    if (!cond->next)
      break;
    Ndb_item *a= cond->next->ndb_item;
    Ndb_item *b, *field, *value= NULL;

    switch (cond->ndb_item->argument_count()) {
    case 1:
      field= (a->type == NDB_FIELD)? a : NULL;
      break;
    case 2:
      if (!cond->next->next)
      {
        field= NULL;
        break;
      }
      b= cond->next->next->ndb_item;
      value= ((a->type == NDB_VALUE) ? a :
              (b->type == NDB_VALUE) ? b :
              NULL);
      field= ((a->type == NDB_FIELD) ? a :
              (b->type == NDB_FIELD) ? b :
              NULL);
      break;
    default:
      field= NULL; //Keep compiler happy
      DBUG_ASSERT(0);
      break;
    }
    switch ((negated) ? 
            Ndb_item::negate(cond->ndb_item->qualification.function_type)
            : cond->ndb_item->qualification.function_type) {
    case NDB_EQ_FUNC:
    {
      if (!value || !field) break;
      // Save value in right format for the field type
      value->save_in_field(field);
      DBUG_PRINT("info", ("Generating EQ filter"));
      if (filter->cmp(NdbScanFilter::COND_EQ, 
                      field->get_field_no(),
                      field->get_val(),
                      field->pack_length()) == -1)
        DBUG_RETURN(1);
      cond= cond->next->next->next;
      DBUG_RETURN(0);
    }
    case NDB_NE_FUNC:
    {
      if (!value || !field) break;
      // Save value in right format for the field type
      value->save_in_field(field);
      DBUG_PRINT("info", ("Generating NE filter"));
      if (filter->cmp(NdbScanFilter::COND_NE, 
                      field->get_field_no(),
                      field->get_val(),
                      field->pack_length()) == -1)
        DBUG_RETURN(1);
      cond= cond->next->next->next;
      DBUG_RETURN(0);
    }
    case NDB_LT_FUNC:
    {
      if (!value || !field) break;
      // Save value in right format for the field type
      value->save_in_field(field);
      if (a == field)
      {
        DBUG_PRINT("info", ("Generating LT filter")); 
        if (filter->cmp(NdbScanFilter::COND_LT, 
                        field->get_field_no(),
                        field->get_val(),
                        field->pack_length()) == -1)
          DBUG_RETURN(1);
      }
      else
      {
        DBUG_PRINT("info", ("Generating GT filter")); 
        if (filter->cmp(NdbScanFilter::COND_GT, 
                        field->get_field_no(),
                        field->get_val(),
                        field->pack_length()) == -1)
          DBUG_RETURN(1);
      }
      cond= cond->next->next->next;
      DBUG_RETURN(0);
    }
    case NDB_LE_FUNC:
    {
      if (!value || !field) break;
      // Save value in right format for the field type
      value->save_in_field(field);
      if (a == field)
      {
        DBUG_PRINT("info", ("Generating LE filter")); 
        if (filter->cmp(NdbScanFilter::COND_LE, 
                        field->get_field_no(),
                        field->get_val(),
                        field->pack_length()) == -1)
          DBUG_RETURN(1);       
      }
      else
      {
        DBUG_PRINT("info", ("Generating GE filter")); 
        if (filter->cmp(NdbScanFilter::COND_GE, 
                        field->get_field_no(),
                        field->get_val(),
                        field->pack_length()) == -1)
          DBUG_RETURN(1);
      }
      cond= cond->next->next->next;
      DBUG_RETURN(0);
    }
    case NDB_GE_FUNC:
    {
      if (!value || !field) break;
      // Save value in right format for the field type
      value->save_in_field(field);
      if (a == field)
      {
        DBUG_PRINT("info", ("Generating GE filter")); 
        if (filter->cmp(NdbScanFilter::COND_GE, 
                        field->get_field_no(),
                        field->get_val(),
                        field->pack_length()) == -1)
          DBUG_RETURN(1);
      }
      else
      {
        DBUG_PRINT("info", ("Generating LE filter")); 
        if (filter->cmp(NdbScanFilter::COND_LE, 
                        field->get_field_no(),
                        field->get_val(),
                        field->pack_length()) == -1)
          DBUG_RETURN(1);
      }
      cond= cond->next->next->next;
      DBUG_RETURN(0);
    }
    case NDB_GT_FUNC:
    {
      if (!value || !field) break;
      // Save value in right format for the field type
      value->save_in_field(field);
      if (a == field)
      {
        DBUG_PRINT("info", ("Generating GT filter"));
        if (filter->cmp(NdbScanFilter::COND_GT, 
                        field->get_field_no(),
                        field->get_val(),
                        field->pack_length()) == -1)
          DBUG_RETURN(1);
      }
      else
      {
        DBUG_PRINT("info", ("Generating LT filter"));
        if (filter->cmp(NdbScanFilter::COND_LT, 
                        field->get_field_no(),
                        field->get_val(),
                        field->pack_length()) == -1)
          DBUG_RETURN(1);
      }
      cond= cond->next->next->next;
      DBUG_RETURN(0);
    }
    case NDB_LIKE_FUNC:
    {
      if (!value || !field) break;
      bool is_string= (value->qualification.value_type == Item::STRING_ITEM);
      // Save value in right format for the field type
      uint32 val_len= value->save_in_field(field);
      char buff[MAX_FIELD_WIDTH];
      String str(buff,sizeof(buff),field->get_field_charset());
      if (val_len > field->get_field()->field_length)
        str.set(value->get_val(), val_len, field->get_field_charset());
      else
        field->get_field_val_str(&str);
      uint32 len=
        ((value->is_const_func() || value->is_cached()) && is_string)?
        str.length():
        value->pack_length();
      const char *val=
        ((value->is_const_func() || value->is_cached()) && is_string)?
        str.ptr()
        : value->get_val();
      DBUG_PRINT("info", ("Generating LIKE filter: like(%d,%s,%d)", 
                          field->get_field_no(),
                          val,
                          len));
      if (filter->cmp(NdbScanFilter::COND_LIKE, 
                      field->get_field_no(),
                      val,
                      len) == -1)
        DBUG_RETURN(1);
      cond= cond->next->next->next;
      DBUG_RETURN(0);
    }
    case NDB_NOTLIKE_FUNC:
    {
      if (!value || !field) break;
      bool is_string= (value->qualification.value_type == Item::STRING_ITEM);
      // Save value in right format for the field type
      uint32 val_len= value->save_in_field(field);
      char buff[MAX_FIELD_WIDTH];
      String str(buff,sizeof(buff),field->get_field_charset());
      if (val_len > field->get_field()->field_length)
        str.set(value->get_val(), val_len, field->get_field_charset());
      else
        field->get_field_val_str(&str);
      uint32 len=
        ((value->is_const_func() || value->is_cached()) && is_string)?
        str.length():
        value->pack_length();
      const char *val=
        ((value->is_const_func() || value->is_cached()) && is_string)?
        str.ptr()
        : value->get_val();
      DBUG_PRINT("info", ("Generating NOTLIKE filter: notlike(%d,%s,%d)", 
                          field->get_field_no(),
                          (value->pack_length() > len)?value->get_val():val,
                          (value->pack_length() > len)?value->pack_length():len));
      if (filter->cmp(NdbScanFilter::COND_NOT_LIKE, 
                      field->get_field_no(),
                      (value->pack_length() > len)?value->get_val():val,
                      (value->pack_length() > len)?value->pack_length():len) == -1)
        DBUG_RETURN(1);
      cond= cond->next->next->next;
      DBUG_RETURN(0);
    }
    case NDB_ISNULL_FUNC:
      if (!field)
        break;
      DBUG_PRINT("info", ("Generating ISNULL filter"));
      if (filter->isnull(field->get_field_no()) == -1)
        DBUG_RETURN(1);
      cond= cond->next->next;
      DBUG_RETURN(0);
    case NDB_ISNOTNULL_FUNC:
    {
      if (!field)
        break;
      DBUG_PRINT("info", ("Generating ISNOTNULL filter"));
      if (filter->isnotnull(field->get_field_no()) == -1)
        DBUG_RETURN(1);         
      cond= cond->next->next;
      DBUG_RETURN(0);
    }
    default:
      break;
    }
    break;
  }
  default:
    break;
  }
  DBUG_PRINT("info", ("Found illegal condition"));
  DBUG_RETURN(1);
}


int
ha_ndbcluster_cond::build_scan_filter_group(Ndb_cond* &cond, 
                                            NdbScanFilter *filter) const
{
  uint level=0;
  bool negated= FALSE;
  DBUG_ENTER("build_scan_filter_group");

  do
  {
    if (!cond)
      DBUG_RETURN(1);
    switch (cond->ndb_item->type) {
    case NDB_FUNCTION:
    {
      switch (cond->ndb_item->qualification.function_type) {
      case NDB_COND_AND_FUNC:
      {
        level++;
        DBUG_PRINT("info", ("Generating %s group %u", (negated)?"NAND":"AND",
                            level));
        if ((negated) ? filter->begin(NdbScanFilter::NAND)
            : filter->begin(NdbScanFilter::AND) == -1)
          DBUG_RETURN(1);
        negated= FALSE;
        cond= cond->next;
        break;
      }
      case NDB_COND_OR_FUNC:
      {
        level++;
        DBUG_PRINT("info", ("Generating %s group %u", (negated)?"NOR":"OR",
                            level));
        if ((negated) ? filter->begin(NdbScanFilter::NOR)
            : filter->begin(NdbScanFilter::OR) == -1)
          DBUG_RETURN(1);
        negated= FALSE;
        cond= cond->next;
        break;
      }
      case NDB_NOT_FUNC:
      {
        DBUG_PRINT("info", ("Generating negated query"));
        cond= cond->next;
        negated= TRUE;
        break;
      }
      default:
        if (build_scan_filter_predicate(cond, filter, negated))
          DBUG_RETURN(1);
        negated= FALSE;
        break;
      }
      break;
    }
    case NDB_END_COND:
      DBUG_PRINT("info", ("End of group %u", level));
      level--;
      if (cond) cond= cond->next;
      if (filter->end() == -1)
        DBUG_RETURN(1);
      if (!negated)
        break;
      // else fall through (NOT END is an illegal condition)
    default:
    {
      DBUG_PRINT("info", ("Illegal scan filter"));
    }
    }
  }  while (level > 0 || negated);
  
  DBUG_RETURN(0);
}


int
ha_ndbcluster_cond::build_scan_filter(Ndb_cond * &cond,
                                      NdbScanFilter *filter) const
{
  bool simple_cond= TRUE;
  DBUG_ENTER("build_scan_filter");  

    switch (cond->ndb_item->type) {
    case NDB_FUNCTION:
      switch (cond->ndb_item->qualification.function_type) {
      case NDB_COND_AND_FUNC:
      case NDB_COND_OR_FUNC:
        simple_cond= FALSE;
        break;
      default:
        break;
      }
      break;
    default:
      break;
    }
  if (simple_cond && filter->begin() == -1)
    DBUG_RETURN(1);
  if (build_scan_filter_group(cond, filter))
    DBUG_RETURN(1);
  if (simple_cond && filter->end() == -1)
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}

int
ha_ndbcluster_cond::generate_scan_filter(NdbInterpretedCode* code,
                                         NdbScanOperation::ScanOptions* options) const
{
  DBUG_ENTER("generate_scan_filter");

  if (m_cond_stack)
  {
    NdbScanFilter filter(code);
    
    int ret= generate_scan_filter_from_cond(filter);
    if (ret != 0)
    {
      const NdbError& err= filter.getNdbError();
      if (err.code == NdbScanFilter::FilterTooLarge)
      {
        // err.message has static storage
        DBUG_PRINT("info", ("%s", err.message));
        push_warning(current_thd, Sql_condition::SL_WARNING,
                     err.code, err.message);
      }
      else
        DBUG_RETURN(ret);
    }
    else if (options!=NULL)
    {
      options->interpretedCode= code;
      options->optionsPresent|= NdbScanOperation::ScanOptions::SO_INTERPRETED;
    }
  }
  else
  {  
    DBUG_PRINT("info", ("Empty stack"));
  }

  DBUG_RETURN(0);
}


int
ha_ndbcluster_cond::generate_scan_filter_from_cond(NdbScanFilter& filter) const
{
  bool multiple_cond= FALSE;
  DBUG_ENTER("generate_scan_filter_from_cond");

  // Wrap an AND group around multiple conditions
  if (m_cond_stack->next) 
  {
    multiple_cond= TRUE;
    if (filter.begin() == -1)
      DBUG_RETURN(1); 
  }
  for (Ndb_cond_stack *stack= m_cond_stack; 
       (stack); 
       stack= stack->next)
  {
    Ndb_cond *cond= stack->ndb_cond;
    
    if (build_scan_filter(cond, &filter))
    {
      DBUG_PRINT("info", ("build_scan_filter failed"));
      DBUG_RETURN(1);
    }
  }
  if (multiple_cond && filter.end() == -1)
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}


/*
  Optimizer sometimes does hash index lookup of a key where some
  key parts are null.  The set of cases where this happens makes
  no sense but cannot be ignored since optimizer may expect the result
  to be filtered accordingly.  The scan is actually on the table and
  the index bounds are pushed down.
*/
int ha_ndbcluster_cond::generate_scan_filter_from_key(NdbInterpretedCode* code,
                                                      NdbScanOperation::ScanOptions* options,
                                                      const KEY* key_info, 
                                                      const key_range *start_key,
                                                      const key_range *end_key) const
{
  DBUG_ENTER("generate_scan_filter_from_key");

#ifndef DBUG_OFF
  {
    DBUG_PRINT("info", ("key parts:%u length:%u",
                        key_info->user_defined_key_parts, key_info->key_length));
    const key_range* keylist[2]={ start_key, end_key };
    for (uint j=0; j <= 1; j++)
    {
      char buf[8192];
      const key_range* key=keylist[j];
      if (key == 0)
      {
        sprintf(buf, "key range %u: none", j);
      }
      else
      {
        sprintf(buf, "key range %u: flag:%u part", j, key->flag);
        const KEY_PART_INFO* key_part=key_info->key_part;
        const uchar* ptr=key->key;
        for (uint i=0; i < key_info->user_defined_key_parts; i++)
        {
          sprintf(buf+strlen(buf), " %u:", i);
          for (uint k=0; k < key_part->store_length; k++)
          {
            sprintf(buf+strlen(buf), " %02x", ptr[k]);
          }
          ptr+=key_part->store_length;
          if (ptr - key->key >= (ptrdiff_t)key->length)
          {
            /*
              key_range has no count of parts so must test byte length.
              But this is not the place for following assert.
            */
            // DBUG_ASSERT(ptr - key->key == key->length);
            break;
          }
          key_part++;
        }
      }
      DBUG_PRINT("info", ("%s", buf));
    }
  }
#endif

  NdbScanFilter filter(code);
  int res;
  filter.begin(NdbScanFilter::AND);
  do
  {
    /*
      Case "x is not null".
      Seen with index(x) where it becomes range "null < x".
      Not seen with index(x,y) for any combination of bounds
      which include "is not null".
    */
    if (start_key != 0 &&
        start_key->flag == HA_READ_AFTER_KEY &&
        end_key == 0 &&
        key_info->user_defined_key_parts == 1)
    {
      const KEY_PART_INFO* key_part=key_info->key_part;
      if (key_part->null_bit != 0) // nullable (must be)
      {
        const uchar* ptr= start_key->key;
        if (ptr[0] != 0) // null (in "null < x")
        {
          DBUG_PRINT("info", ("Generating ISNOTNULL filter for nullable %s",
                              key_part->field->field_name));
          if (filter.isnotnull(key_part->fieldnr-1) == -1)
            DBUG_RETURN(1);
          break;
        }
      }
    }

    /*
      Case "x is null" in an EQ range.
      Seen with index(x) for "x is null".
      Seen with index(x,y) for "x is null and y = 1".
      Not seen with index(x,y) for "x is null and y is null".
      Seen only when all key parts are present (but there is
      no reason to limit the code to this case).
    */
    if (start_key != 0 &&
        start_key->flag == HA_READ_KEY_EXACT &&
        end_key != 0 &&
        end_key->flag == HA_READ_AFTER_KEY &&
        start_key->length == end_key->length &&
        memcmp(start_key->key, end_key->key, start_key->length) == 0)
    {
      const KEY_PART_INFO* key_part=key_info->key_part;
      const uchar* ptr=start_key->key;
      for (uint i=0; i < key_info->user_defined_key_parts; i++)
      {
        const Field* field=key_part->field;
        if (key_part->null_bit) // nullable
        {
          if (ptr[0] != 0) // null
          {
            DBUG_PRINT("info", ("Generating ISNULL filter for nullable %s",
                                field->field_name));
            if (filter.isnull(key_part->fieldnr-1) == -1)
              DBUG_RETURN(1);
          }
          else
          {
            DBUG_PRINT("info", ("Generating EQ filter for nullable %s",
                                field->field_name));
            if (filter.cmp(NdbScanFilter::COND_EQ, 
                           key_part->fieldnr-1,
                           ptr + 1, // skip null-indicator byte
                           field->pack_length()) == -1)
              DBUG_RETURN(1);
          }
        }
        else
        {
          DBUG_PRINT("info", ("Generating EQ filter for non-nullable %s",
                              field->field_name));
          if (filter.cmp(NdbScanFilter::COND_EQ, 
                         key_part->fieldnr-1,
                         ptr,
                         field->pack_length()) == -1)
            DBUG_RETURN(1);
        }
        ptr+=key_part->store_length;
        if (ptr - start_key->key >= (ptrdiff_t)start_key->length)
        {
          break;
        }
        key_part++;
      }
      break;
    }

    DBUG_PRINT("info", ("Unknown hash index scan"));
    // enable to catch new cases when optimizer changes
    // DBUG_ASSERT(false);
  }
  while (0);

  // Add any pushed condition
  if (m_cond_stack &&
      (res= generate_scan_filter_from_cond(filter)))
    DBUG_RETURN(res);
    
  if (filter.end() == -1)
    DBUG_RETURN(1);
  
  if (options!=NULL)
  {
    options->interpretedCode= code;
    options->optionsPresent|= NdbScanOperation::ScanOptions::SO_INTERPRETED;
  }

  DBUG_RETURN(0);
}
