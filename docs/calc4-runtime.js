var Module="undefined"!==typeof Module?Module:{},objAssign=Object.assign,moduleOverrides=objAssign({},Module),arguments_=[],thisProgram="./this.program",quit_=(a,b)=>{throw b;},ENVIRONMENT_IS_WEB="object"===typeof window,ENVIRONMENT_IS_WORKER="function"===typeof importScripts,ENVIRONMENT_IS_NODE="object"===typeof process&&"object"===typeof process.versions&&"string"===typeof process.versions.node,scriptDirectory="";
function locateFile(a){return Module.locateFile?Module.locateFile(a,scriptDirectory):scriptDirectory+a}var read_,readAsync,readBinary,setWindowTitle;function logExceptionOnExit(a){a instanceof ExitStatus||err("exiting due to exception: "+a)}var fs,nodePath,requireNodeFS;
if(ENVIRONMENT_IS_NODE)scriptDirectory=ENVIRONMENT_IS_WORKER?require("path").dirname(scriptDirectory)+"/":__dirname+"/",requireNodeFS=function(){nodePath||(fs=require("fs"),nodePath=require("path"))},read_=function(a,b){requireNodeFS();a=nodePath.normalize(a);return fs.readFileSync(a,b?null:"utf8")},readBinary=function(a){a=read_(a,!0);a.buffer||(a=new Uint8Array(a));return a},readAsync=function(a,b,c){requireNodeFS();a=nodePath.normalize(a);fs.readFile(a,function(d,f){d?c(d):b(f.buffer)})},1<process.argv.length&&
(thisProgram=process.argv[1].replace(/\\/g,"/")),arguments_=process.argv.slice(2),"undefined"!==typeof module&&(module.exports=Module),process.on("uncaughtException",function(a){if(!(a instanceof ExitStatus))throw a;}),process.on("unhandledRejection",function(a){throw a;}),quit_=(a,b)=>{if(keepRuntimeAlive())throw process.exitCode=a,b;logExceptionOnExit(b);process.exit(a)},Module.inspect=function(){return"[Emscripten Module object]"};else if(ENVIRONMENT_IS_WEB||ENVIRONMENT_IS_WORKER)ENVIRONMENT_IS_WORKER?
scriptDirectory=self.location.href:"undefined"!==typeof document&&document.currentScript&&(scriptDirectory=document.currentScript.src),scriptDirectory=0!==scriptDirectory.indexOf("blob:")?scriptDirectory.substr(0,scriptDirectory.replace(/[?#].*/,"").lastIndexOf("/")+1):"",read_=function(a){var b=new XMLHttpRequest;b.open("GET",a,!1);b.send(null);return b.responseText},ENVIRONMENT_IS_WORKER&&(readBinary=function(a){var b=new XMLHttpRequest;b.open("GET",a,!1);b.responseType="arraybuffer";b.send(null);
return new Uint8Array(b.response)}),readAsync=function(a,b,c){var d=new XMLHttpRequest;d.open("GET",a,!0);d.responseType="arraybuffer";d.onload=function(){200==d.status||0==d.status&&d.response?b(d.response):c()};d.onerror=c;d.send(null)},setWindowTitle=a=>document.title=a;var out=Module.print||console.log.bind(console),err=Module.printErr||console.warn.bind(console);objAssign(Module,moduleOverrides);moduleOverrides=null;Module.arguments&&(arguments_=Module.arguments);
Module.thisProgram&&(thisProgram=Module.thisProgram);Module.quit&&(quit_=Module.quit);var tempRet0=0,setTempRet0=function(a){tempRet0=a},getTempRet0=function(){return tempRet0},wasmBinary;Module.wasmBinary&&(wasmBinary=Module.wasmBinary);var noExitRuntime=Module.noExitRuntime||!0;"object"!==typeof WebAssembly&&abort("no native wasm support detected");var wasmMemory,ABORT=!1,EXITSTATUS;function getCFunc(a){return Module["_"+a]}
function ccall(a,b,c,d,f){f={string:function(l){var q=0;if(null!==l&&void 0!==l&&0!==l){var r=(l.length<<2)+1;q=stackAlloc(r);stringToUTF8(l,q,r)}return q},array:function(l){var q=stackAlloc(l.length);writeArrayToMemory(l,q);return q}};a=getCFunc(a);var g=[],h=0;if(d)for(var k=0;k<d.length;k++){var n=f[c[k]];n?(0===h&&(h=stackSave()),g[k]=n(d[k])):g[k]=d[k]}c=a.apply(null,g);return c=function(l){0!==h&&stackRestore(h);l="string"===b?UTF8ToString(l):"boolean"===b?!!l:l;return l}(c)}
function cwrap(a,b,c,d){c=c||[];var f=c.every(function(g){return"number"===g});return"string"!==b&&f&&!d?getCFunc(a):function(){return ccall(a,b,c,arguments,d)}}var UTF8Decoder="undefined"!==typeof TextDecoder?new TextDecoder("utf8"):void 0;
function UTF8ArrayToString(a,b,c){var d=b+c;for(c=b;a[c]&&!(c>=d);)++c;if(16<c-b&&a.subarray&&UTF8Decoder)return UTF8Decoder.decode(a.subarray(b,c));for(d="";b<c;){var f=a[b++];if(f&128){var g=a[b++]&63;if(192==(f&224))d+=String.fromCharCode((f&31)<<6|g);else{var h=a[b++]&63;f=224==(f&240)?(f&15)<<12|g<<6|h:(f&7)<<18|g<<12|h<<6|a[b++]&63;65536>f?d+=String.fromCharCode(f):(f-=65536,d+=String.fromCharCode(55296|f>>10,56320|f&1023))}}else d+=String.fromCharCode(f)}return d}
function UTF8ToString(a,b){return a?UTF8ArrayToString(HEAPU8,a,b):""}
function stringToUTF8Array(a,b,c,d){if(!(0<d))return 0;var f=c;d=c+d-1;for(var g=0;g<a.length;++g){var h=a.charCodeAt(g);if(55296<=h&&57343>=h){var k=a.charCodeAt(++g);h=65536+((h&1023)<<10)|k&1023}if(127>=h){if(c>=d)break;b[c++]=h}else{if(2047>=h){if(c+1>=d)break;b[c++]=192|h>>6}else{if(65535>=h){if(c+2>=d)break;b[c++]=224|h>>12}else{if(c+3>=d)break;b[c++]=240|h>>18;b[c++]=128|h>>12&63}b[c++]=128|h>>6&63}b[c++]=128|h&63}}b[c]=0;return c-f}
function stringToUTF8(a,b,c){return stringToUTF8Array(a,HEAPU8,b,c)}function lengthBytesUTF8(a){for(var b=0,c=0;c<a.length;++c){var d=a.charCodeAt(c);55296<=d&&57343>=d&&(d=65536+((d&1023)<<10)|a.charCodeAt(++c)&1023);127>=d?++b:b=2047>=d?b+2:65535>=d?b+3:b+4}return b}function writeArrayToMemory(a,b){HEAP8.set(a,b)}function writeAsciiToMemory(a,b,c){for(var d=0;d<a.length;++d)HEAP8[b++>>0]=a.charCodeAt(d);c||(HEAP8[b>>0]=0)}var buffer,HEAP8,HEAPU8,HEAP16,HEAPU16,HEAP32,HEAPU32,HEAPF32,HEAPF64;
function updateGlobalBufferAndViews(a){buffer=a;Module.HEAP8=HEAP8=new Int8Array(a);Module.HEAP16=HEAP16=new Int16Array(a);Module.HEAP32=HEAP32=new Int32Array(a);Module.HEAPU8=HEAPU8=new Uint8Array(a);Module.HEAPU16=HEAPU16=new Uint16Array(a);Module.HEAPU32=HEAPU32=new Uint32Array(a);Module.HEAPF32=HEAPF32=new Float32Array(a);Module.HEAPF64=HEAPF64=new Float64Array(a)}
var INITIAL_MEMORY=Module.INITIAL_MEMORY||16777216,wasmTable,__ATPRERUN__=[],__ATINIT__=[],__ATPOSTRUN__=[],runtimeInitialized=!1,runtimeKeepaliveCounter=0;function keepRuntimeAlive(){return noExitRuntime||0<runtimeKeepaliveCounter}function preRun(){if(Module.preRun)for("function"==typeof Module.preRun&&(Module.preRun=[Module.preRun]);Module.preRun.length;)addOnPreRun(Module.preRun.shift());callRuntimeCallbacks(__ATPRERUN__)}
function initRuntime(){runtimeInitialized=!0;callRuntimeCallbacks(__ATINIT__)}function postRun(){if(Module.postRun)for("function"==typeof Module.postRun&&(Module.postRun=[Module.postRun]);Module.postRun.length;)addOnPostRun(Module.postRun.shift());callRuntimeCallbacks(__ATPOSTRUN__)}function addOnPreRun(a){__ATPRERUN__.unshift(a)}function addOnInit(a){__ATINIT__.unshift(a)}function addOnPostRun(a){__ATPOSTRUN__.unshift(a)}var runDependencies=0,runDependencyWatcher=null,dependenciesFulfilled=null;
function addRunDependency(a){runDependencies++;Module.monitorRunDependencies&&Module.monitorRunDependencies(runDependencies)}function removeRunDependency(a){runDependencies--;Module.monitorRunDependencies&&Module.monitorRunDependencies(runDependencies);0==runDependencies&&(null!==runDependencyWatcher&&(clearInterval(runDependencyWatcher),runDependencyWatcher=null),dependenciesFulfilled&&(a=dependenciesFulfilled,dependenciesFulfilled=null,a()))}Module.preloadedImages={};Module.preloadedAudios={};
function abort(a){if(Module.onAbort)Module.onAbort(a);a="Aborted("+a+")";err(a);ABORT=!0;EXITSTATUS=1;throw new WebAssembly.RuntimeError(a+". Build with -s ASSERTIONS=1 for more info.");}var dataURIPrefix="data:application/octet-stream;base64,";function isDataURI(a){return a.startsWith(dataURIPrefix)}function isFileURI(a){return a.startsWith("file://")}var wasmBinaryFile;wasmBinaryFile="calc4-runtime-core.wasm";isDataURI(wasmBinaryFile)||(wasmBinaryFile=locateFile(wasmBinaryFile));
function getBinary(a){try{if(a==wasmBinaryFile&&wasmBinary)return new Uint8Array(wasmBinary);if(readBinary)return readBinary(a);throw"both async and sync fetching of the wasm failed";}catch(b){abort(b)}}
function getBinaryPromise(){if(!wasmBinary&&(ENVIRONMENT_IS_WEB||ENVIRONMENT_IS_WORKER)){if("function"===typeof fetch&&!isFileURI(wasmBinaryFile))return fetch(wasmBinaryFile,{credentials:"same-origin"}).then(function(a){if(!a.ok)throw"failed to load wasm binary file at '"+wasmBinaryFile+"'";return a.arrayBuffer()}).catch(function(){return getBinary(wasmBinaryFile)});if(readAsync)return new Promise(function(a,b){readAsync(wasmBinaryFile,function(c){a(new Uint8Array(c))},b)})}return Promise.resolve().then(function(){return getBinary(wasmBinaryFile)})}
function createWasm(){function a(f,g){Module.asm=f.exports;wasmMemory=Module.asm.X;updateGlobalBufferAndViews(wasmMemory.buffer);wasmTable=Module.asm._;addOnInit(Module.asm.Y);removeRunDependency("wasm-instantiate")}function b(f){a(f.instance)}function c(f){return getBinaryPromise().then(function(g){return WebAssembly.instantiate(g,d)}).then(function(g){return g}).then(f,function(g){err("failed to asynchronously prepare wasm: "+g);abort(g)})}var d={a:asmLibraryArg};addRunDependency("wasm-instantiate");
if(Module.instantiateWasm)try{return Module.instantiateWasm(d,a)}catch(f){return err("Module.instantiateWasm callback failed with error: "+f),!1}(function(){return wasmBinary||"function"!==typeof WebAssembly.instantiateStreaming||isDataURI(wasmBinaryFile)||isFileURI(wasmBinaryFile)||"function"!==typeof fetch?c(b):fetch(wasmBinaryFile,{credentials:"same-origin"}).then(function(f){return WebAssembly.instantiateStreaming(f,d).then(b,function(g){err("wasm streaming compile failed: "+g);err("falling back to ArrayBuffer instantiation");
return c(b)})})})();return{}}function callRuntimeCallbacks(a){for(;0<a.length;){var b=a.shift();if("function"==typeof b)b(Module);else{var c=b.func;"number"===typeof c?void 0===b.arg?getWasmTableEntry(c)():getWasmTableEntry(c)(b.arg):c(void 0===b.arg?null:b.arg)}}}var wasmTableMirror=[];function getWasmTableEntry(a){var b=wasmTableMirror[a];b||(a>=wasmTableMirror.length&&(wasmTableMirror.length=a+1),wasmTableMirror[a]=b=wasmTable.get(a));return b}
function ___cxa_allocate_exception(a){return _malloc(a+16)+16}
function ExceptionInfo(a){this.excPtr=a;this.ptr=a-16;this.set_type=function(b){HEAP32[this.ptr+4>>2]=b};this.get_type=function(){return HEAP32[this.ptr+4>>2]};this.set_destructor=function(b){HEAP32[this.ptr+8>>2]=b};this.get_destructor=function(){return HEAP32[this.ptr+8>>2]};this.set_refcount=function(b){HEAP32[this.ptr>>2]=b};this.set_caught=function(b){HEAP8[this.ptr+12>>0]=b?1:0};this.get_caught=function(){return 0!=HEAP8[this.ptr+12>>0]};this.set_rethrown=function(b){HEAP8[this.ptr+13>>0]=b?
1:0};this.get_rethrown=function(){return 0!=HEAP8[this.ptr+13>>0]};this.init=function(b,c){this.set_type(b);this.set_destructor(c);this.set_refcount(0);this.set_caught(!1);this.set_rethrown(!1)};this.add_ref=function(){HEAP32[this.ptr>>2]+=1};this.release_ref=function(){var b=HEAP32[this.ptr>>2];HEAP32[this.ptr>>2]=b-1;return 1===b}}
function CatchInfo(a){this.free=function(){_free(this.ptr);this.ptr=0};this.set_base_ptr=function(b){HEAP32[this.ptr>>2]=b};this.get_base_ptr=function(){return HEAP32[this.ptr>>2]};this.set_adjusted_ptr=function(b){HEAP32[this.ptr+4>>2]=b};this.get_adjusted_ptr_addr=function(){return this.ptr+4};this.get_adjusted_ptr=function(){return HEAP32[this.ptr+4>>2]};this.get_exception_ptr=function(){if(___cxa_is_pointer_type(this.get_exception_info().get_type()))return HEAP32[this.get_base_ptr()>>2];var b=
this.get_adjusted_ptr();return 0!==b?b:this.get_base_ptr()};this.get_exception_info=function(){return new ExceptionInfo(this.get_base_ptr())};void 0===a?(this.ptr=_malloc(8),this.set_adjusted_ptr(0)):this.ptr=a}var exceptionCaught=[];function exception_addRef(a){a.add_ref()}var uncaughtExceptionCount=0;
function ___cxa_begin_catch(a){a=new CatchInfo(a);var b=a.get_exception_info();b.get_caught()||(b.set_caught(!0),uncaughtExceptionCount--);b.set_rethrown(!1);exceptionCaught.push(a);exception_addRef(b);return a.get_exception_ptr()}var exceptionLast=0;function ___cxa_free_exception(a){try{return _free((new ExceptionInfo(a)).ptr)}catch(b){}}
function exception_decRef(a){if(a.release_ref()&&!a.get_rethrown()){var b=a.get_destructor();b&&getWasmTableEntry(b)(a.excPtr);___cxa_free_exception(a.excPtr)}}function ___cxa_end_catch(){_setThrew(0);var a=exceptionCaught.pop();exception_decRef(a.get_exception_info());a.free();exceptionLast=0}function ___resumeException(a){a=new CatchInfo(a);var b=a.get_base_ptr();exceptionLast||=b;a.free();throw b;}
function ___cxa_find_matching_catch_2(){var a=exceptionLast;if(!a)return setTempRet0(0),0;var b=(new ExceptionInfo(a)).get_type(),c=new CatchInfo;c.set_base_ptr(a);c.set_adjusted_ptr(a);if(!b)return setTempRet0(0),c.ptr|0;a=Array.prototype.slice.call(arguments);for(var d=0;d<a.length;d++){var f=a[d];if(0===f||f===b)break;if(___cxa_can_catch(f,b,c.get_adjusted_ptr_addr()))return setTempRet0(f),c.ptr|0}setTempRet0(b);return c.ptr|0}
function ___cxa_find_matching_catch_3(){var a=exceptionLast;if(!a)return setTempRet0(0),0;var b=(new ExceptionInfo(a)).get_type(),c=new CatchInfo;c.set_base_ptr(a);c.set_adjusted_ptr(a);if(!b)return setTempRet0(0),c.ptr|0;a=Array.prototype.slice.call(arguments);for(var d=0;d<a.length;d++){var f=a[d];if(0===f||f===b)break;if(___cxa_can_catch(f,b,c.get_adjusted_ptr_addr()))return setTempRet0(f),c.ptr|0}setTempRet0(b);return c.ptr|0}
function ___cxa_find_matching_catch_5(){var a=exceptionLast;if(!a)return setTempRet0(0),0;var b=(new ExceptionInfo(a)).get_type(),c=new CatchInfo;c.set_base_ptr(a);c.set_adjusted_ptr(a);if(!b)return setTempRet0(0),c.ptr|0;a=Array.prototype.slice.call(arguments);for(var d=0;d<a.length;d++){var f=a[d];if(0===f||f===b)break;if(___cxa_can_catch(f,b,c.get_adjusted_ptr_addr()))return setTempRet0(f),c.ptr|0}setTempRet0(b);return c.ptr|0}
function ___cxa_rethrow(){var a=exceptionCaught.pop();a||abort("no exception to throw");var b=a.get_exception_info(),c=a.get_base_ptr();b.get_rethrown()?a.free():(exceptionCaught.push(a),b.set_rethrown(!0),b.set_caught(!1),uncaughtExceptionCount++);exceptionLast=c;throw c;}function ___cxa_throw(a,b,c){(new ExceptionInfo(a)).init(b,c);exceptionLast=a;uncaughtExceptionCount++;throw a;}function ___cxa_uncaught_exceptions(){return uncaughtExceptionCount}function _abort(){abort("")}var _emscripten_get_now;
_emscripten_get_now=ENVIRONMENT_IS_NODE?()=>{var a=process.hrtime();return 1E3*a[0]+a[1]/1E6}:()=>performance.now();var _emscripten_get_now_is_monotonic=!0;function setErrNo(a){return HEAP32[___errno_location()>>2]=a}function _clock_gettime(a,b){if(0===a)a=Date.now();else{if(1!==a&&4!==a||!_emscripten_get_now_is_monotonic)return setErrNo(28),-1;a=_emscripten_get_now()}HEAP32[b>>2]=a/1E3|0;HEAP32[b+4>>2]=a%1E3*1E6|0;return 0}function _emscripten_memcpy_big(a,b,c){HEAPU8.copyWithin(a,b,b+c)}
function abortOnCannotGrowMemory(a){abort("OOM")}function _emscripten_resize_heap(a){abortOnCannotGrowMemory(a>>>0)}var ENV={};function getExecutableName(){return thisProgram||"./this.program"}
function getEnvStrings(){if(!getEnvStrings.strings){var a={USER:"web_user",LOGNAME:"web_user",PATH:"/",PWD:"/",HOME:"/home/web_user",LANG:("object"===typeof navigator&&navigator.languages&&navigator.languages[0]||"C").replace("-","_")+".UTF-8",_:getExecutableName()},b;for(b in ENV)void 0===ENV[b]?delete a[b]:a[b]=ENV[b];var c=[];for(b in a)c.push(b+"="+a[b]);getEnvStrings.strings=c}return getEnvStrings.strings}
var SYSCALLS={mappings:{},buffers:[null,[],[]],printChar:function(a,b){var c=SYSCALLS.buffers[a];0===b||10===b?((1===a?out:err)(UTF8ArrayToString(c,0)),c.length=0):c.push(b)},varargs:void 0,get:function(){SYSCALLS.varargs+=4;return HEAP32[SYSCALLS.varargs-4>>2]},getStr:function(a){return UTF8ToString(a)},get64:function(a,b){return a}};function _environ_get(a,b){var c=0;getEnvStrings().forEach(function(d,f){var g=b+c;HEAP32[a+4*f>>2]=g;writeAsciiToMemory(d,g);c+=d.length+1});return 0}
function _environ_sizes_get(a,b){var c=getEnvStrings();HEAP32[a>>2]=c.length;var d=0;c.forEach(function(f){d+=f.length+1});HEAP32[b>>2]=d;return 0}function _getTempRet0(){return getTempRet0()}function _llvm_eh_typeid_for(a){return a}function _setTempRet0(a){setTempRet0(a)}function __isLeapYear(a){return 0===a%4&&(0!==a%100||0===a%400)}function __arraySum(a,b){for(var c=0,d=0;d<=b;c+=a[d++]);return c}
var __MONTH_DAYS_LEAP=[31,29,31,30,31,30,31,31,30,31,30,31],__MONTH_DAYS_REGULAR=[31,28,31,30,31,30,31,31,30,31,30,31];function __addDays(a,b){for(a=new Date(a.getTime());0<b;){var c=__isLeapYear(a.getFullYear()),d=a.getMonth();c=(c?__MONTH_DAYS_LEAP:__MONTH_DAYS_REGULAR)[d];if(b>c-a.getDate())b-=c-a.getDate()+1,a.setDate(1),11>d?a.setMonth(d+1):(a.setMonth(0),a.setFullYear(a.getFullYear()+1));else{a.setDate(a.getDate()+b);break}}return a}
function _strftime(a,b,c,d){function f(e,m,p){for(e="number"===typeof e?e.toString():e||"";e.length<m;)e=p[0]+e;return e}function g(e,m){return f(e,m,"0")}function h(e,m){function p(v){return 0>v?-1:0<v?1:0}var u;0===(u=p(e.getFullYear()-m.getFullYear()))&&0===(u=p(e.getMonth()-m.getMonth()))&&(u=p(e.getDate()-m.getDate()));return u}function k(e){switch(e.getDay()){case 0:return new Date(e.getFullYear()-1,11,29);case 1:return e;case 2:return new Date(e.getFullYear(),0,3);case 3:return new Date(e.getFullYear(),
0,2);case 4:return new Date(e.getFullYear(),0,1);case 5:return new Date(e.getFullYear()-1,11,31);case 6:return new Date(e.getFullYear()-1,11,30)}}function n(e){e=__addDays(new Date(e.tm_year+1900,0,1),e.tm_yday);var m=new Date(e.getFullYear(),0,4),p=new Date(e.getFullYear()+1,0,4);m=k(m);p=k(p);return 0>=h(m,e)?0>=h(p,e)?e.getFullYear()+1:e.getFullYear():e.getFullYear()-1}var l=HEAP32[d+40>>2];d={tm_sec:HEAP32[d>>2],tm_min:HEAP32[d+4>>2],tm_hour:HEAP32[d+8>>2],tm_mday:HEAP32[d+12>>2],tm_mon:HEAP32[d+
16>>2],tm_year:HEAP32[d+20>>2],tm_wday:HEAP32[d+24>>2],tm_yday:HEAP32[d+28>>2],tm_isdst:HEAP32[d+32>>2],tm_gmtoff:HEAP32[d+36>>2],tm_zone:l?UTF8ToString(l):""};c=UTF8ToString(c);l={"%c":"%a %b %d %H:%M:%S %Y","%D":"%m/%d/%y","%F":"%Y-%m-%d","%h":"%b","%r":"%I:%M:%S %p","%R":"%H:%M","%T":"%H:%M:%S","%x":"%m/%d/%y","%X":"%H:%M:%S","%Ec":"%c","%EC":"%C","%Ex":"%m/%d/%y","%EX":"%H:%M:%S","%Ey":"%y","%EY":"%Y","%Od":"%d","%Oe":"%e","%OH":"%H","%OI":"%I","%Om":"%m","%OM":"%M","%OS":"%S","%Ou":"%u","%OU":"%U",
"%OV":"%V","%Ow":"%w","%OW":"%W","%Oy":"%y"};for(var q in l)c=c.replace(new RegExp(q,"g"),l[q]);var r="Sunday Monday Tuesday Wednesday Thursday Friday Saturday".split(" "),t="January February March April May June July August September October November December".split(" ");l={"%a":function(e){return r[e.tm_wday].substring(0,3)},"%A":function(e){return r[e.tm_wday]},"%b":function(e){return t[e.tm_mon].substring(0,3)},"%B":function(e){return t[e.tm_mon]},"%C":function(e){return g((e.tm_year+1900)/100|
0,2)},"%d":function(e){return g(e.tm_mday,2)},"%e":function(e){return f(e.tm_mday,2," ")},"%g":function(e){return n(e).toString().substring(2)},"%G":function(e){return n(e)},"%H":function(e){return g(e.tm_hour,2)},"%I":function(e){e=e.tm_hour;0==e?e=12:12<e&&(e-=12);return g(e,2)},"%j":function(e){return g(e.tm_mday+__arraySum(__isLeapYear(e.tm_year+1900)?__MONTH_DAYS_LEAP:__MONTH_DAYS_REGULAR,e.tm_mon-1),3)},"%m":function(e){return g(e.tm_mon+1,2)},"%M":function(e){return g(e.tm_min,2)},"%n":function(){return"\n"},
"%p":function(e){return 0<=e.tm_hour&&12>e.tm_hour?"AM":"PM"},"%S":function(e){return g(e.tm_sec,2)},"%t":function(){return"\t"},"%u":function(e){return e.tm_wday||7},"%U":function(e){var m=new Date(e.tm_year+1900,0,1),p=0===m.getDay()?m:__addDays(m,7-m.getDay());e=new Date(e.tm_year+1900,e.tm_mon,e.tm_mday);return 0>h(p,e)?(m=__arraySum(__isLeapYear(e.getFullYear())?__MONTH_DAYS_LEAP:__MONTH_DAYS_REGULAR,e.getMonth()-1)-31,p=31-p.getDate()+m+e.getDate(),g(Math.ceil(p/7),2)):0===h(p,m)?"01":"00"},
"%V":function(e){var m=new Date(e.tm_year+1901,0,4),p=k(new Date(e.tm_year+1900,0,4));m=k(m);var u=__addDays(new Date(e.tm_year+1900,0,1),e.tm_yday);if(0>h(u,p))return"53";if(0>=h(m,u))return"01";e=p.getFullYear()<e.tm_year+1900?e.tm_yday+32-p.getDate():e.tm_yday+1-p.getDate();return g(Math.ceil(e/7),2)},"%w":function(e){return e.tm_wday},"%W":function(e){var m=new Date(e.tm_year,0,1),p=1===m.getDay()?m:__addDays(m,0===m.getDay()?1:7-m.getDay()+1);e=new Date(e.tm_year+1900,e.tm_mon,e.tm_mday);return 0>
h(p,e)?(m=__arraySum(__isLeapYear(e.getFullYear())?__MONTH_DAYS_LEAP:__MONTH_DAYS_REGULAR,e.getMonth()-1)-31,p=31-p.getDate()+m+e.getDate(),g(Math.ceil(p/7),2)):0===h(p,m)?"01":"00"},"%y":function(e){return(e.tm_year+1900).toString().substring(2)},"%Y":function(e){return e.tm_year+1900},"%z":function(e){e=e.tm_gmtoff;var m=0<=e;e=Math.abs(e)/60;return(m?"+":"-")+String("0000"+(e/60*100+e%60)).slice(-4)},"%Z":function(e){return e.tm_zone},"%%":function(){return"%"}};for(q in l)c.includes(q)&&(c=c.replace(new RegExp(q,
"g"),l[q](d)));q=intArrayFromString(c,!1);if(q.length>b)return 0;writeArrayToMemory(q,a);return q.length-1}function _strftime_l(a,b,c,d){return _strftime(a,b,c,d)}function intArrayFromString(a,b,c){c=0<c?c:lengthBytesUTF8(a)+1;c=Array(c);a=stringToUTF8Array(a,c,0,c.length);b&&(c.length=a);return c}
var asmLibraryArg={n:___cxa_allocate_exception,r:___cxa_begin_catch,u:___cxa_end_catch,b:___cxa_find_matching_catch_2,i:___cxa_find_matching_catch_3,m:___cxa_find_matching_catch_5,o:___cxa_free_exception,H:___cxa_rethrow,t:___cxa_throw,V:___cxa_uncaught_exceptions,d:___resumeException,D:_abort,W:_clock_gettime,P:_emscripten_memcpy_big,Q:_emscripten_resize_heap,S:_environ_get,T:_environ_sizes_get,a:_getTempRet0,E:invoke_diii,F:invoke_fiii,p:invoke_i,c:invoke_ii,J:invoke_iid,f:invoke_iii,g:invoke_iiii,
l:invoke_iiiii,U:invoke_iiiiid,z:invoke_iiiiii,v:invoke_iiiiiii,G:invoke_iiiiiiii,B:invoke_iiiiiiiiiiii,L:invoke_iiiiij,O:invoke_iij,M:invoke_j,N:invoke_jii,K:invoke_jiiii,j:invoke_v,k:invoke_vi,e:invoke_vii,h:invoke_viii,q:invoke_viiii,x:invoke_viiiii,w:invoke_viiiiii,s:invoke_viiiiiii,y:invoke_viiiiiiiiii,A:invoke_viiiiiiiiiiiiiii,I:_llvm_eh_typeid_for,C:_setTempRet0,R:_strftime_l},asm=createWasm(),___wasm_call_ctors=Module.___wasm_call_ctors=function(){return(___wasm_call_ctors=Module.___wasm_call_ctors=
Module.asm.Y).apply(null,arguments)},_Execute=Module._Execute=function(){return(_Execute=Module._Execute=Module.asm.Z).apply(null,arguments)},___errno_location=Module.___errno_location=function(){return(___errno_location=Module.___errno_location=Module.asm.$).apply(null,arguments)},_setThrew=Module._setThrew=function(){return(_setThrew=Module._setThrew=Module.asm.aa).apply(null,arguments)},stackSave=Module.stackSave=function(){return(stackSave=Module.stackSave=Module.asm.ba).apply(null,arguments)},
stackRestore=Module.stackRestore=function(){return(stackRestore=Module.stackRestore=Module.asm.ca).apply(null,arguments)},stackAlloc=Module.stackAlloc=function(){return(stackAlloc=Module.stackAlloc=Module.asm.da).apply(null,arguments)},_malloc=Module._malloc=function(){return(_malloc=Module._malloc=Module.asm.ea).apply(null,arguments)},_free=Module._free=function(){return(_free=Module._free=Module.asm.fa).apply(null,arguments)},___cxa_can_catch=Module.___cxa_can_catch=function(){return(___cxa_can_catch=
Module.___cxa_can_catch=Module.asm.ga).apply(null,arguments)},___cxa_is_pointer_type=Module.___cxa_is_pointer_type=function(){return(___cxa_is_pointer_type=Module.___cxa_is_pointer_type=Module.asm.ha).apply(null,arguments)},dynCall_iij=Module.dynCall_iij=function(){return(dynCall_iij=Module.dynCall_iij=Module.asm.ia).apply(null,arguments)},dynCall_jii=Module.dynCall_jii=function(){return(dynCall_jii=Module.dynCall_jii=Module.asm.ja).apply(null,arguments)},dynCall_j=Module.dynCall_j=function(){return(dynCall_j=
Module.dynCall_j=Module.asm.ka).apply(null,arguments)},dynCall_iiiiij=Module.dynCall_iiiiij=function(){return(dynCall_iiiiij=Module.dynCall_iiiiij=Module.asm.la).apply(null,arguments)},dynCall_jiiii=Module.dynCall_jiiii=function(){return(dynCall_jiiii=Module.dynCall_jiiii=Module.asm.ma).apply(null,arguments)};function invoke_vii(a,b,c){var d=stackSave();try{getWasmTableEntry(a)(b,c)}catch(f){stackRestore(d);if(f!==f+0&&"longjmp"!==f)throw f;_setThrew(1,0)}}
function invoke_ii(a,b){var c=stackSave();try{return getWasmTableEntry(a)(b)}catch(d){stackRestore(c);if(d!==d+0&&"longjmp"!==d)throw d;_setThrew(1,0)}}function invoke_viii(a,b,c,d){var f=stackSave();try{getWasmTableEntry(a)(b,c,d)}catch(g){stackRestore(f);if(g!==g+0&&"longjmp"!==g)throw g;_setThrew(1,0)}}function invoke_iiii(a,b,c,d){var f=stackSave();try{return getWasmTableEntry(a)(b,c,d)}catch(g){stackRestore(f);if(g!==g+0&&"longjmp"!==g)throw g;_setThrew(1,0)}}
function invoke_iii(a,b,c){var d=stackSave();try{return getWasmTableEntry(a)(b,c)}catch(f){stackRestore(d);if(f!==f+0&&"longjmp"!==f)throw f;_setThrew(1,0)}}function invoke_viiii(a,b,c,d,f){var g=stackSave();try{getWasmTableEntry(a)(b,c,d,f)}catch(h){stackRestore(g);if(h!==h+0&&"longjmp"!==h)throw h;_setThrew(1,0)}}function invoke_iid(a,b,c){var d=stackSave();try{return getWasmTableEntry(a)(b,c)}catch(f){stackRestore(d);if(f!==f+0&&"longjmp"!==f)throw f;_setThrew(1,0)}}
function invoke_vi(a,b){var c=stackSave();try{getWasmTableEntry(a)(b)}catch(d){stackRestore(c);if(d!==d+0&&"longjmp"!==d)throw d;_setThrew(1,0)}}function invoke_v(a){var b=stackSave();try{getWasmTableEntry(a)()}catch(c){stackRestore(b);if(c!==c+0&&"longjmp"!==c)throw c;_setThrew(1,0)}}function invoke_iiiiiii(a,b,c,d,f,g,h){var k=stackSave();try{return getWasmTableEntry(a)(b,c,d,f,g,h)}catch(n){stackRestore(k);if(n!==n+0&&"longjmp"!==n)throw n;_setThrew(1,0)}}
function invoke_viiiiii(a,b,c,d,f,g,h){var k=stackSave();try{getWasmTableEntry(a)(b,c,d,f,g,h)}catch(n){stackRestore(k);if(n!==n+0&&"longjmp"!==n)throw n;_setThrew(1,0)}}function invoke_viiiii(a,b,c,d,f,g){var h=stackSave();try{getWasmTableEntry(a)(b,c,d,f,g)}catch(k){stackRestore(h);if(k!==k+0&&"longjmp"!==k)throw k;_setThrew(1,0)}}
function invoke_iiiii(a,b,c,d,f){var g=stackSave();try{return getWasmTableEntry(a)(b,c,d,f)}catch(h){stackRestore(g);if(h!==h+0&&"longjmp"!==h)throw h;_setThrew(1,0)}}function invoke_iiiiii(a,b,c,d,f,g){var h=stackSave();try{return getWasmTableEntry(a)(b,c,d,f,g)}catch(k){stackRestore(h);if(k!==k+0&&"longjmp"!==k)throw k;_setThrew(1,0)}}
function invoke_iiiiid(a,b,c,d,f,g){var h=stackSave();try{return getWasmTableEntry(a)(b,c,d,f,g)}catch(k){stackRestore(h);if(k!==k+0&&"longjmp"!==k)throw k;_setThrew(1,0)}}function invoke_iiiiiiii(a,b,c,d,f,g,h,k){var n=stackSave();try{return getWasmTableEntry(a)(b,c,d,f,g,h,k)}catch(l){stackRestore(n);if(l!==l+0&&"longjmp"!==l)throw l;_setThrew(1,0)}}
function invoke_fiii(a,b,c,d){var f=stackSave();try{return getWasmTableEntry(a)(b,c,d)}catch(g){stackRestore(f);if(g!==g+0&&"longjmp"!==g)throw g;_setThrew(1,0)}}function invoke_diii(a,b,c,d){var f=stackSave();try{return getWasmTableEntry(a)(b,c,d)}catch(g){stackRestore(f);if(g!==g+0&&"longjmp"!==g)throw g;_setThrew(1,0)}}function invoke_i(a){var b=stackSave();try{return getWasmTableEntry(a)()}catch(c){stackRestore(b);if(c!==c+0&&"longjmp"!==c)throw c;_setThrew(1,0)}}
function invoke_viiiiiii(a,b,c,d,f,g,h,k){var n=stackSave();try{getWasmTableEntry(a)(b,c,d,f,g,h,k)}catch(l){stackRestore(n);if(l!==l+0&&"longjmp"!==l)throw l;_setThrew(1,0)}}function invoke_iiiiiiiiiiii(a,b,c,d,f,g,h,k,n,l,q,r){var t=stackSave();try{return getWasmTableEntry(a)(b,c,d,f,g,h,k,n,l,q,r)}catch(e){stackRestore(t);if(e!==e+0&&"longjmp"!==e)throw e;_setThrew(1,0)}}
function invoke_viiiiiiiiii(a,b,c,d,f,g,h,k,n,l,q){var r=stackSave();try{getWasmTableEntry(a)(b,c,d,f,g,h,k,n,l,q)}catch(t){stackRestore(r);if(t!==t+0&&"longjmp"!==t)throw t;_setThrew(1,0)}}function invoke_viiiiiiiiiiiiiii(a,b,c,d,f,g,h,k,n,l,q,r,t,e,m,p){var u=stackSave();try{getWasmTableEntry(a)(b,c,d,f,g,h,k,n,l,q,r,t,e,m,p)}catch(v){stackRestore(u);if(v!==v+0&&"longjmp"!==v)throw v;_setThrew(1,0)}}
function invoke_iij(a,b,c,d){var f=stackSave();try{return dynCall_iij(a,b,c,d)}catch(g){stackRestore(f);if(g!==g+0&&"longjmp"!==g)throw g;_setThrew(1,0)}}function invoke_jii(a,b,c){var d=stackSave();try{return dynCall_jii(a,b,c)}catch(f){stackRestore(d);if(f!==f+0&&"longjmp"!==f)throw f;_setThrew(1,0)}}function invoke_j(a){var b=stackSave();try{return dynCall_j(a)}catch(c){stackRestore(b);if(c!==c+0&&"longjmp"!==c)throw c;_setThrew(1,0)}}
function invoke_iiiiij(a,b,c,d,f,g,h){var k=stackSave();try{return dynCall_iiiiij(a,b,c,d,f,g,h)}catch(n){stackRestore(k);if(n!==n+0&&"longjmp"!==n)throw n;_setThrew(1,0)}}function invoke_jiiii(a,b,c,d,f){var g=stackSave();try{return dynCall_jiiii(a,b,c,d,f)}catch(h){stackRestore(g);if(h!==h+0&&"longjmp"!==h)throw h;_setThrew(1,0)}}Module.cwrap=cwrap;var calledRun;function ExitStatus(a){this.name="ExitStatus";this.message="Program terminated with exit("+a+")";this.status=a}
dependenciesFulfilled=function runCaller(){calledRun||run();calledRun||(dependenciesFulfilled=runCaller)};function run(a){function b(){if(!calledRun&&(calledRun=!0,Module.calledRun=!0,!ABORT)){initRuntime();if(Module.onRuntimeInitialized)Module.onRuntimeInitialized();postRun()}}0<runDependencies||(preRun(),0<runDependencies||(Module.setStatus?(Module.setStatus("Running..."),setTimeout(function(){setTimeout(function(){Module.setStatus("")},1);b()},1)):b()))}Module.run=run;
if(Module.preInit)for("function"==typeof Module.preInit&&(Module.preInit=[Module.preInit]);0<Module.preInit.length;)Module.preInit.pop()();run();
Module.onRuntimeInitialized=async a=>{let b={execute:Module.cwrap("Execute","string",["string","bool","bool"])};self.addEventListener("message",c=>{try{console.log("Worker - Got message, start execution: "+c.data);let d=b.execute(c.data.source,c.data.optimize,c.data.dump);console.log("Worker - Finished execution");self.postMessage({success:!0,result:d});console.log("Worker - Sent a message")}catch(d){console.log("Worker - Exception: "+d.message),self.postMessage({success:!1,result:"Fatal error: "+
d.message})}});console.log("Worker - Initialized");self.postMessage({initialized:!0});console.log("Worker - Sent a initialization message")};
