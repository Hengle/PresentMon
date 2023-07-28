const { defineConfig } = require('@vue/cli-service')
module.exports = defineConfig({
  transpileDependencies: [
    'vuetify'
  ],
  filenameHashing: false,
  productionSourceMap: false,
  configureWebpack: {    
    resolve: {
      fallback: {
        "fs": false,
        "tls": false,
        "net": false,
        "path": false,
        "zlib": false,
        "http": false,
        "https": false,
        "stream": false,
        "crypto": false,
        "child_process": false,
        "os": false,
        "tty": false,
        "url": false,
        "util": false,
        "crypto-browserify": false, 
      }
    }
  }
})
