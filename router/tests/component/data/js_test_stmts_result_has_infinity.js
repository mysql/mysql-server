// ensure that 'stmts.result.columns[].decimal = Infinity' throws

({
  stmts: [
    {
      result: {
        columns: [
          {
            name: "foo",
            type: "STRING",
            decimals: Infinity
          }
        ]
      }
    }
  ]
})
