// ensure that 'stmts.result.columns[].decimal = -1' throws

({stmts: [{result: {columns: [{name: "foo", type: "STRING", decimals: -1}]}}]})
