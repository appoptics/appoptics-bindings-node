'use strict'

var fs = require('fs')
var releaseInfo = require('linux-os-info')

const dir = './oboe/'
const soname = 'liboboe-1.0.so.0'

// don't delete unused yet. old versions of npm (3.10.10) don't
// have prepare and prepublishOnly which are needed to sequence
// downloading/installing liboboe and the running setup-liboboe
// to get the links right.
const deleteUnused = false

//
// both versions of the .so library are included in the package.
//

const oboeNames = {
  alpine: 'liboboe-1.0-alpine-x86_64.so.0.0.0',
  linux: 'liboboe-1.0-x86_64.so.0.0.0'
}

function setupLiboboe (cb) {
  const version = fs.readFileSync(dir + 'VERSION', 'utf8').slice(0, -1)

  releaseInfo().then(info => {
    if (info.platform !== 'linux') {
      console.error(`appoptics-apm runs on linux; the ${info.platform} platform is not supported`)
      process.exit(1)
    }
    return info.id === 'alpine' ? 'alpine' : 'linux'
  }).then(linux => {
    const liboboeName = oboeNames[linux]

    // must find the library
    if (!fs.existsSync(dir + liboboeName)) {
      throw new Error('cannot find ' + dir + liboboeName)
    }

    //
    // setup the symbolic links to the right version of liboboe
    //

    // remove links if they exist
    if (isSymbolicLink(dir + 'liboboe.so')) {
      fs.unlinkSync(dir + 'liboboe.so')
    }

    if (isSymbolicLink(dir + soname)) {
      fs.unlinkSync(dir + soname)
    }

    fs.symlinkSync(liboboeName, dir + 'liboboe.so')
    fs.symlinkSync(liboboeName, dir + soname)

    // delete the unused file if indicated. ignore errors.
    if (deleteUnused) {
      let unusedOboe = oboeNames[linux === 'alpine' ? 'linux' : 'alpine']
      // do async so errors can easily be ignored
      fs.unlink(dir + unusedOboe, function (err) {
        console.log(err)
        process.exit(0)
      })
    }
    process.exit(0)

  }).catch(e => {
    console.error(e)
    process.exit(1)
  })
}

function isSymbolicLink (name) {
  try {
    if (fs.lstatSync(name).isSymbolicLink()) {
      return true
    }
  } catch (e) {
    // ignore errors
  }
  return false
}

if (!module.parent) {
  var i = setInterval(function() {}, 100)
  setupLiboboe(function () {clearInterval(i)})
} else {
  module.exports = setupLiboboe
}
