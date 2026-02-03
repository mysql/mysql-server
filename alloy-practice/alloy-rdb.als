//一般的なRDBの構造定義および制約

//ベースの定義；テーブル，レコード，列，値
sig Table {}
sig Column { table: one Table }
sig Record {
	table: one Table,
	values: Column -> lone Value
}
sig Value {}
one sig Null extends Value {}

//構造制約；レコードの値が紐ずくカラムはテーブルに定義されているカラムに含まれる必要がある
//1. 全てのcolmnに対応するValue(null含む)がある
//2. 親となるTable以外のcolmnを参照しない
fact RecordUsesTableColumns { 
	all r : Record |
		r.values.Value = { c : Column | c.table = r.table }
		//r.values.Value in { c : Column | c.table = r.table }	: 1を保証しない
		//{ c : Column | c.table = r.table } in r.values.Value	: 2を保証しない
}

run Table {}