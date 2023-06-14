({
  handshake: {
    auth: {
      username: 'username',
      password: 'password',
      certificate: {
        subject:
            '/C=IN/ST=Karnataka/L=Bengaluru/O=Oracle/OU=MySQL/CN=MySQL CRL test client certificate',
      },
    }
  },
  stmts: function() {
    return {
      error: {}
    }
  }
})
