// ensure that 'stmts.result.columns[].repeat' throws

({
  stmts: [
    {
      result: {
        columns: [
          {
            name: "foo",
            type: "STRING",
            repeat: 25
          }
        ]
      }
    }
  ]
})
