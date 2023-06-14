({
  handshake: {
    auth: {
      username: 'username',
      password: 'password',
      certificate: {
        issuer:
            '/C=IN/ST=Karnataka/L=Bengaluru/O=Oracle/OU=MySQL/CN=MySQL CRL test ca certificate',
      },
    }
  },
  stmts: function() {
    return {
      error: {}
    }
  }
})
