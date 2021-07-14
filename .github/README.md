# @appoptics/apm-bindings

[@appoptics/apm-bindings](https://www.npmjs.com/package/@appoptics/apm-bindings) is an NPM package containing a binary node add-on.


The package is installed as a dependency when the AppOptics APM agent ([appoptics-apm](https://github.com/appoptics/appoptics-apm-node)) is installed. In any install run it will first attempt to install a prebuilt add-on using [node-pre-gyp](https://github.com/mapbox/node-pre-gyp) and only if that fails, will it attempt to build the add-on from source using [node-gyp](https://github.com/nodejs/node-gyp).

This is a **Linux Only package**. No Mac or Windows support.

## Two minutes on how it works

[@appoptics/apm-bindings](https://github.com/appoptics/appoptics-bindings-node) implements a low-level interface to `liboboe`, a closed-source library maintained by SolarWinds. `liboboe` implements communications and aggregation functions to enable efficient sampling of traces. Traces are sequences of entry and exit events which capture performance information. 

Each event has a:

```
label: (string - typically entry or exit)
span: (string - the name of the span being instrumented)
timestamp: (integer - unix timestamp)
hostname: (string)
X-trace ID: <header><task ID><op ID><flags>
edge: the previous event's X-trace ID
```

An X-trace ID (XID) comprises a single byte header with the hex value `2B`, a task ID of 20 bytes that is the same for all events in a trace, an op ID of 8 bytes that is unique for each event in a trace, and a flag byte. In this implementation the XID is manipulated as binary data (via Metadata and Event objects) or as a string of 60 hex digits. As a string, the header is 2 characters, the task ID is 40 characters, the op ID is 16 characters, and the flag byte is 2 characters.

Note: layer is a legacy term for span. It has been replaced in the code but still appears in event messages as Layer and in the liboboe interface.

## Local Development

Development **must be done on Linux**.

Building with node-gyp requires:
* Python v3.6, v3.7, v3.8, or v3.9
* make
* A proper C/C++ compiler toolchain, like [GCC](https://gcc.gnu.org/)

### Project layout

* `src` directory contains the C++ code to bind to liboboe. 
* `oboe` directory contains liboboe and its required header files. liboboe is downloaded from: https://files.appoptics.com/c-lib/10.1.1/. Pre-release versions are at: https://s3-us-west-2.amazonaws.com/rc-files-t2/c-lib
* `test` directory contains the test suite. 
* `.github` contains the files for github actions.

### With Docker:

1. Create a `.env` file and set: `AO_TOKEN_PROD={a valid production token}`. Potentially you can also set `AO_TOKEN_STG={a valid staging token}`
2. Run `npm run dev` - this will create a docker container, set it up, and open a shell.
3. Run `npm test` to make sure all is ok.

### Testing

Test are run using [Mocha](https://github.com/mochajs/mocha).

1. Source `. env.sh prod` to set up the environment variables to work against the real appoptics collector. 
2. Execute `npm test` to run the test suite.

The following environment variables must be set for the "should get verification that a request should be sampled" test. `env.sh` will set them up for you if you have defined AO_TOKEN_PROD.

- `APPOPTICS_COLLECTOR=collector.appoptics.com:443`
- `APPOPTICS_SERVICE_KEY=<a valid service key for the appoptics>:name`

To run tests in other configurations use `. env.sh stg`, `. env.sh udp`

The `test` script in `package.json` runs `test.sh` which then manages how mocha runs each test file.

### Building

Building is done using [node-pre-gyp](https://github.com/mapbox/node-pre-gyp).

1. `npx node-pre-gyp rebuild` is all that is needed. More granular commands avaliable. See node-pre-gyp documentation.

Before a build, `setup-liboboe.js` must run in order to create symbolic links to the correct version of liboboe so the `SONAME` field can be satisfied. 

The `install` and `rebuild` scripts in `package.json` run `setup-liboboe.js` as the first step before invoking node-pre-gyp. As a result, initial `npm` install will set links as required.

`setup-liboboe.js` can be run multiple times and may be run again if needs be.

### Debugging

Debugging node addons is not intuitive but this might help (from [stackoverflow](https://stackoverflow.com/questions/23228868/how-to-debug-binary-module-of-nodejs))

First, compile your add-on using node-gyp with the --debug flag.

`node-gyp --debug configure rebuild`

(The next point about changing the require path doesn't apply to appoptics-bindings because it uses the `bindings` module and that will find the module in `Debug`, `Release`, and other locations.)

Second, if you're still in "playground" mode like I am, you're probably loading your module with something like

`var ObjModule = require('./ObjModule/build/Release/objModule');`

However, when you rebuild using node-gyp in debug mode, node-gyp throws away the Release version and creates a Debug version instead. So update the module path:

`var ObjModule = require('./ObjModule/build/Debug/objModule');`

Alright, now we're ready to debug our C++ add-on. Run gdb against the node binary, which is a C++ application. Now, node itself doesn't know about your add-on, so when you try to set a breakpoint on your add-on function (in this case, StringReverse) it complains that the specific function is not defined. Fear not, your add-on is part of the "future shared library load" it refers to, and will be loaded once you require() your add-on in JavaScript.

```
$ gdb node
...
Reading symbols from node...done.
(gdb) break StringReverse
Function "StringReverse" not defined.
Make breakpoint pending on future shared library load? (y or [n]) y
```

OK, now we just have to run the application:

```
(gdb) run ../modTest.js
...
Breakpoint 1, StringReverse (args=...) at ../objModule.cpp:49
```

If a signal is thrown gdb will stop on the line generating it.

Finally, here's a link to using output formats (and the whole set of gdb docs) [gdb](http://www.delorie.com/gnu/docs/gdb/gdb_55.html).


#### Miscellaneous

Note: use `tail` if you only want to see the highest version required, leave it off to see all.

Find the highest version of GLIBCXX is supported in /usr/lib/libstdc++.so.?

`readelf -sV /usr/lib/libstdc++.so.6 | sed -n 's/.*@@GLIBCXX_//p' | sort -u -V | tail -1`

Find the versions of GLIBCXX required by a file

`readelf -sV build/Release/appoptics-bindings.node | sed -n 's/^.*\(@GLIBCXX_[^ ]*\).*$/\1/p' | sort -u -V`
`objdump -T /lib/x86_64-linux-gnu/libc.so.6 | sed -n 's/^.*\(GLIBCXX_[^ ]*\).*$/\1/p' | sort -u -V`

Dump a `.node` file as asm (build debug for better symbols):


`objdump -CRrS build/Release/ao-metrics.node  > ao-metrics.s`


## Development & Release with GitHub Actions 

> **tl;dr** Push to feature branch. Create Pull Request. Merge Pull Request. Push version tag to release.

### Overview

The package is `node-pre-gyp` enabled and is published in a two step process. First prebuilt add-on tarballs are uploaded to an S3 bucket, and then an NPM package is published to the NPM registry. Prebuilt tarballs must be versioned with the same version as the NPM package and they must be present in the S3 bucket prior to the NPM package itself being published to the registry.

There are many platforms that can use the prebuilt add-on but will fail to build it, hence the importance of the prebuilts.

### Use

#### Workflows and Events

##### Prep - Push Dockerfile

* Push to master is disabled by branch protection.
* Push to branch which changes any Dockerfile in the `.github/docker-node/` directory will trigger [docker-node.yml](./workflows/docker-node.yml). 
* Workflow will:
  - Build all create a [single package](https://github.com/appoptics/appoptics-bindings-node/pkgs/container/appoptics-bindings-node%2Fnode) named `node` scoped to `appoptics/appoptics-bindings-node` (the repo). Package has multiple tagged images for each of the dockerfiles from which it was built. For example, the image created from a file named ``10-centos7-build.Dockerfile` has a `10-centos7-build` tag and can pulled from `ghcr.io/appoptics/appoptics-bindings-node/node:10-centos7-build`. Since this repo is public, the images are also public.
* Workflow creates (or recreates) images used in other workflows.
* Manual trigger supported.

##### Develop - Push

* Push to master is disabled by branch protection.
* Push to branch will trigger [push.yml](./workflows/push.yml). 
* Workflow will:
  - Build the code pushed on a default image. (`node` image from docker hub).
  - Run the tests against the build.
* Workflow confirms code is not "broken".
* Manual trigger supported. Enables to select image from docker hub or local files (e.g. `erbium-buster-slim`) to build code on.

##### Review - Pull Request

* Creating a pull request will trigger [review.yml](./workflows/review.yml). 
* Workflow will:
  - Build the code pushed on each of the Build Group images. 
  - Run the tests on each build.
* Workflow confirms code can be built in each of the required variations.
* Manual trigger supported. 

##### Accept - Merge Pull Request 

* Merging a pull request will trigger [accept.yml](./workflows/accept.yml). 
* Workflow will:
  - Clear the *staging* S3* bucket of prebuilt tarballs (if exist for version).
  - Create all Fallback Group images and install. Since prebuilt tarball has been cleared, install will fallback to build from source.
  - Build the code pushed on each of the Build Group images.
  - Package the built code and upload a tarball to the *staging* S3 bucket. 
  - Create all Prebuilt Group images and install the prebuilt tarball on each.
* Workflow ensures node-pre-gyp setup (config and S3 buckets) is working for a wide variety of potential customer configurations.
* Manual trigger supported. Enables to select running the tests after install (on both Fallback & Prebuilt groups)

##### Release - Push Version Tag

* Release process is `npm` and `git` triggered.
* To Release:
  1. On branch run `npm version {major/minor/patch}`(e.g. `npm version patch`) then have the branch pass through the Push/Pull/Merge flow above. 
  2. When ready `git push` origin {tag name} (e.g. `git push origin v11.2.3`).
* Pushing a semantic versioning tag for a patch/minor/major versions (e.g. `v11.2.3`) or an alpha tagged pre-release (e.g. `v11.2.3-alpha.2`) will trigger [release.yml](./workflows/release.yml). Pushing other pre-release tags (e.g. `v11.2.3-7`) is ignored.
* Workflow will: 
  - Build the code pushed in each of the Build Group images. 
  - Package the built code and upload a tarball to the *production* S3 bucket. 
  - Create all Target Group images and install the prebuilt tarball on each.
  - Publish an NPM package upon successful completion of all steps above. When version tag is `alpha`, package will be NPM tagged same. When it is a release version, package will be NPM tagged `latest`.
* Workflow ensures node-pre-gyp setup is working in *production* for a wide variety of potential customer configurations.
* Workflow publishing to NPM registry exposes the NPM package (and the prebuilt tarballs in the *production* S3 bucket) to the public.
* Note: @appoptics/apm-bindings is not meant to be directly consumed. It is developed as a dependency of [appoptics-apm](https://www.npmjs.com/package/appoptics-apm).

#### Workflow Diagram

##### Prep Workflow
```
push Dockerfile ─► ┌───────────────────┐ ─► ─► ─► ─► ─►
                   │Build Docker Images│ build & publish
manual ──────────► └───────────────────┘      │
                                              ▼
                                          Test Workflows
```



##### Test Workflows
```
push to branch ──► ┌───────────────────┐ ─► ─► ─► ─► ─►
                   │Single Build & Test│ contained build
manual (image?) ─► └───────────────────┘ ◄── ◄── ◄── ◄──


pull request ────► ┌──────────────────┐ ─► ─► ─► ─► ─►
                   │Group Build & Test│ contained build
manual ──────────► └──────────────────┘ ◄── ◄── ◄── ◄──

merge to master ─► ┌──────────────────────┐
                   │Fallback Group Install│
manual (test?) ──► └┬─────────────────────┘
                    │
                    │   ┌───────────────────────────┐ ─► ─► ─►
                    └─► │Build Group Build & Package│ S3 Package
                        └┬──────────────────────────┘ Staging
                         │
                         │   ┌──────────────────────┐     │
                         └─► │Prebuilt Group Install│ ◄── ▼
                             └──────────────────────┘

```

##### Release Workflow

```
push semver tag ─► ┌────────────────────────────┐ ─► ─► ─►
push alpha tag     │Build Group Build & Package │ S3 Package
                   └┬───────────────────────────┘ Production
                    │
                    │   ┌────────────────────┐     │
                    └─► │Target Group Install│ ◄── ▼
                        └┬───────────────────┘
                         │
                         │   ┌───────────┐
                         └─► │NPM Publish│
                             └───────────┘
```

### Maintain

> **tl;dr** There is no need to modify workflows. All data used is externalized.

#### Definitions
* Local images are defined in [docker-node](./docker-node).
* S3 Staging bucket is defined in [package.json](../package.json).
* S3 Production bucket is defined in [package.json](../package.json).
* [Build Group](./config/build-group.yml) are images on which the various versions of the add-on are built. They include combinations to support different Node versions and libc implementations. Generally build is done with the lowest versions of the OSes supported, so that `glibc`/`musl` versions are the oldest/most compatible.
* [Fallback Group](./config/fallback-group.yml) images include OS and Node version combinations that *can* build for source.
* [Prebuilt Group](./config/prebuilt-group.yml) images include OS and Node version combinations that *can not* build for source and thus require a prebuilt tarball.
* [Target Group](./config/target-group.yml) images include a wide variety of OS and Node version combinations. Group includes both images that can build from code as well as those which can not.

#### Adding an image to GitHub Container Registry

1. Create a docker file with a unique name to be used as a tag. Common is to use: `{node-version}-{os-name-version}` (e.g `16-ubuntu20.04.2.Dockerfile`). If image is a build image suffix with `-build`.
2. Place a Docker file in the `docker-node` directory.

#### Modifying group lists

1. Find available tags at [Docker Hub](https://hub.docker.com/_/node) or use path of image published to GitHub Container Registry (e.g. `ghcr.io/$GITHUB_REPOSITORY/node:14-centos7`)
2. Add to appropriate group json file in `config`.

#### Adding a Node Version

1. Create an `alpine` builder image and a `centos` builder image. Use previous node version Dockerfiles as guide.
2. Add to appropriate group json files in `config`.

#### Remove a node version

1. Remove version images from appropriate group json file in `config`.
2. Leave `docker-node` Dockerfiles for future reference.

### Implementation

> **tl;dr** No Actions used. Simple script gets image, returns running container id. Steps exec on container.

#### Workflows

1. All workflows `runs-on: ubuntu-latest`.
2. For maintainability and security custom actions are avoided and shell scripts preferred.
3. Configuration has been externalized. All images groups are loaded from external json files located in the `config` directory.
4. Loading uses [fromJSON function and a standard two-job setup](https://docs.github.com/en/actions/reference/context-and-expression-syntax-for-github-actions#fromjson).
5. Loading is encapsulated in s shell script (`matrix-from-json.sh`). Since the script is not a "formal" action it is placed in a `script` directory.
3. All job steps are named.
4. `matrix` and `container` directives are used for groups.
5. Jobs are linked using `needs:`.

#### Secrets

1. Repo is defined with 6 secrets:
```
AO_TOKEN_PROD
AWS_ACCESS_KEY_ID
AWS_SECRET_ACCESS_KEY
PROD_AWS_ACCESS_KEY_ID
PROD_AWS_SECRET_ACCESS_KEY
NPM_AUTH_TOKEN
```

Fabriqué au Canada : Made in Canada 🇨🇦
