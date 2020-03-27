'use strict';

const net = require('net');
const bindings = require('../..')
const expect = require('chai').expect;
const fs = require('fs');
const util = require('util');

const EnvVarOptions = require('../lib/env-var-options');
const keyMap = require('../lib/env-var-key-map');

//
// goodOptions are used in multiple tests so they're declared here
//
const goodOptions = {
  serviceKey: `${process.env.AO_SWOKEN_PROD}:node-oboe-notifier`,
  traceMetrics: true,
};

const notiSocket = '/tmp/ao-notifications';
let notiServer;
let notiClient;

const messages = [];

let messageConsumer;

describe('addon.oboeInit()', function () {
  before(function (done) {
    fs.unlink(notiSocket, function () {
      done();
    });
  });
  // before calling oboe init setup the notification server
  before (function (done) {
    messageConsumer = msg => {
      if (msg.source !== 'oboe' || msg.type !== 'keep-alive') {
        throw new Error(`unexpected message ${msg}`);
      }
      done();
    };
    notiServer = net.createServer(function (client) {
      if (notiClient) {
        throw new Error('more than one client connection');
      }
      notiClient = client;
      client.on('end', function () {
        notiClient = undefined;
      });

      let previousData = '';
      client.on('data', function (data) {
        previousData = previousData + data.toString('utf8');
        let ix;
        while ((ix = previousData.indexOf('\n')) >= 0) {
          const json = previousData.substring(0, ix);
          previousData = previousData.substring(ix + 1);
          const msg = JSON.parse(json);
          messageConsumer(msg);
        }
      });
    });

    notiServer.on('error', function (e) {
      throw e;
    });
    notiServer.listen(notiSocket);
    notiServer.unref();

    const status = bindings.oboeNotifierInit(notiSocket);
    if (status !== 0 && status !== -2) {
      throw new Error(`oboeNotifierInit() returned ${status}`);
    }
  });

  it('oboeInit() should successfully complete', function () {
    // replace the message consumer for the rest of the tests
    messageConsumer = msg => messages.push(msg);

    const details = {};
    const options = Object.assign({}, goodOptions);
    const expected = options;
    const result = bindings.oboeInit(options, details);

    expect(result).lte(0, 'oboeInit() not get a version mismatch');
    expect(details.valid).deep.equal(expected);
    expect(details.processed).deep.equal(expected);

    const keysIn = Object.keys(options);
    const keysOut = Object.keys(details.processed);
    expect(keysIn.length).equal(keysOut.length);
  });

  it('should receive an oboe logging message', function (done) {
    let counter = 0;
    const id = setInterval(function () {
      if (messages.length) {
        clearInterval(id);
        const msg = messages.shift();
        expect(msg.seqNo).equal(1, 'should have a seqNo of 1');
        expect(msg.source).equal('oboe', 'source didn\'t match');
        expect(msg.type).equal('logging', 'oboe message should be type logging');
        expect(msg.level).equal('info', 'the logging level should be info');
        done();
      } else {
        counter += 1;
        if (counter > 10) {
          clearInterval(id);
          throw new Error('no message arrived');
        }
      }
    }, 0);
  })

  it('should receive a collector remote-config message', function (done) {
    let counter = 0;
    const id = setInterval(function () {
      if (messages.length) {
        clearInterval(id);
        const msg = messages.shift();
        expect(msg.seqNo).equal(2, 'should have a seqNo of 2');
        expect(msg.source).equal('collector');
        expect(msg.type).equal('remote-config', 'type should be remote-config');
        expect(msg).property('config', 'MetricsFlushInterval');
        expect(msg).property('value', 60);
        done();
      } else {
        counter += 1;
        if (counter > 10) {
          clearInterval(id);
          throw new Error('no message arrived');
        }
      }
    }, 500);
  });

  it('should receive an oboe keep-alive message', function (done) {
    let counter = 0;
    const id = setInterval(function () {
      if (messages.length) {
        clearInterval(id);
        const msg = messages.shift();
        expect(msg.seqNo).equal(3, 'should have a seqNo of 3');
        expect(msg.source).equal('oboe');
        expect(msg.type).equal('keep-alive');
        done();
      } else {
        counter += 1;
        if (counter > 10) {
          clearInterval(id);
          throw new Error('no message arrived');
        }
      }
    }, 10000);
  });

  it('should shutdown correctly via client.destroy()', function (done) {
    messageConsumer = msg => {
      throw new Error(`unexpected message arrived: ${util.format(msg)}`);
    }

    let endEmitted = false;
    notiClient.end(function (e) {
      bindings.oboeNotifierStop();
      endEmitted = true;
      notiClient.destroy();
    });

    // this should generate the end callback.
    notiServer.close();

    // wait a minute to make sure no keep-alives come in.
    setTimeout(function () {
      expect(endEmitted).equal(true, 'the end event should have happened');
      done();
    }, 61000)
  });

})
