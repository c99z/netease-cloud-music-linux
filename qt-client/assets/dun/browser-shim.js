// Minimal browser surface for the Yidun Watchman SDK. Host hooks (provided by the
// embedding engine): __host.http(method, url, body, headers, cb(status, text)),
// __host.loadScript(url, cb(err)), __host.timer(ms, cb) -> id, __host.clearTimer(id),
// __host.storage (object persisted between runs), __host.now().
var window = this;
window.globalThis = window;
var self = window, top = window, parent = window, globalThis_ = window;
window.window = window; window.self = window; window.top = window; window.parent = window;
window.setTimeout = function (f, ms) { var a = Array.prototype.slice.call(arguments, 2); return __host.timer(ms || 0, function () { if (typeof f === 'function') f.apply(window, a); }); };
window.clearTimeout = function (id) { __host.clearTimer(id); };
window.setInterval = function (f, ms) { var id = {}; var tick = function () { id.t = __host.timer(ms || 1, function () { f(); tick(); }); }; tick(); return id; };
window.clearInterval = function (id) { if (id) __host.clearTimer(id.t); };
window.devicePixelRatio = 1;
window.innerWidth = 1920; window.innerHeight = 969; window.outerWidth = 1920; window.outerHeight = 1050;
window.screen = { width: 1920, height: 1080, availWidth: 1920, availHeight: 1040, colorDepth: 24, pixelDepth: 24 };
window.navigator = {
  userAgent: 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36',
  appCodeName: 'Mozilla', appName: 'Netscape',
  appVersion: '5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36',
  platform: 'MacIntel', product: 'Gecko', productSub: '20030107', vendor: 'Google Inc.',
  language: 'zh-CN', languages: ['zh-CN', 'zh'], cookieEnabled: true, doNotTrack: null,
  hardwareConcurrency: 8, maxTouchPoints: 0, webdriver: false,
  plugins: { length: 5 }, mimeTypes: { length: 2 },
  javaEnabled: function () { return false; }
};
window.location = { href: 'https://music.163.com/', protocol: 'https:', host: 'music.163.com', hostname: 'music.163.com', port: '', pathname: '/', search: '', hash: '', origin: 'https://music.163.com' };
window.history = { length: 1 };
window.performance = { timing: { navigationStart: __host.now() - 1500, loadEventEnd: __host.now() - 200 }, navigation: { type: 0, redirectCount: 0 }, now: function () { return 1500; } };
window.chrome = { runtime: {} };
window.external = {};
window.localStorage = {
  getItem: function (k) { return Object.prototype.hasOwnProperty.call(__host.storage, k) ? String(__host.storage[k]) : null; },
  setItem: function (k, v) { __host.storage[k] = String(v); __host.persist(); },
  removeItem: function (k) { delete __host.storage[k]; __host.persist(); }
};
window.sessionStorage = { _d: {}, getItem: function (k) { return this._d.hasOwnProperty(k) ? this._d[k] : null; }, setItem: function (k, v) { this._d[k] = String(v); }, removeItem: function (k) { delete this._d[k]; } };
window.addEventListener = function () {}; window.removeEventListener = function () {};
window.encodeURIComponent = encodeURIComponent; window.decodeURIComponent = decodeURIComponent;
window.escape = escape; window.unescape = unescape;

function Element(tag) { this.tagName = String(tag).toUpperCase(); this.nodeName = this.tagName; this.style = {}; this.childNodes = []; this.attributes = {}; this.parentNode = null; }
Element.prototype.setAttribute = function (k, v) { this.attributes[k] = String(v); if (k === 'src') this.src = v; };
Element.prototype.getAttribute = function (k) { return this.attributes.hasOwnProperty(k) ? this.attributes[k] : null; };
Element.prototype.removeAttribute = function (k) { delete this.attributes[k]; };
Element.prototype.appendChild = function (c) {
  c.parentNode = this; this.childNodes.push(c);
  if (c.tagName === 'SCRIPT' && c.src) {
    __host.loadScript(c.src, function (err) {
      if (err) { if (c.onerror) c.onerror(new Error(err)); }
      else { if (c.onload) c.onload(); if (c.onreadystatechange) { c.readyState = 'loaded'; c.onreadystatechange(); } }
    });
  }
  return c;
};
Element.prototype.insertBefore = function (c) { return this.appendChild(c); };
Element.prototype.removeChild = function (c) { var i = this.childNodes.indexOf(c); if (i >= 0) this.childNodes.splice(i, 1); c.parentNode = null; return c; };
Element.prototype.addEventListener = function () {}; Element.prototype.removeEventListener = function () {};
Element.prototype.getElementsByTagName = function () { return []; };
Element.prototype.getContext = function () { return null; };   // no canvas fingerprint, like jsdom
Element.prototype.toDataURL = function () { return 'data:,'; };
Object.defineProperty(Element.prototype, 'firstChild', { get: function () { return this.childNodes[0] || null; } });

var head = new Element('head'), body = new Element('body'), html = new Element('html');
html.appendChild(head); html.appendChild(body);
var cookieJar = {};
window.document = {
  documentElement: html, head: head, body: body, readyState: 'complete', compatMode: 'CSS1Compat',
  contentType: 'text/html', characterSet: 'UTF-8', referrer: 'https://music.163.com/', URL: 'https://music.163.com/',
  location: window.location, defaultView: window,
  createElement: function (tag) { return new Element(tag); },
  createEvent: function (t) { if (t === 'TouchEvent') throw new Error('NotSupportedError'); return {}; },
  getElementById: function () { return null; },
  getElementsByTagName: function (t) { t = String(t).toLowerCase(); if (t === 'head') return [head]; if (t === 'body') return [body]; if (t === 'script') return head.childNodes.filter(function (n) { return n.tagName === 'SCRIPT'; }); return []; },
  querySelector: function () { return null; }, querySelectorAll: function () { return []; },
  addEventListener: function () {}, removeEventListener: function () {},
  get cookie() { return Object.keys(cookieJar).map(function (k) { return k + '=' + cookieJar[k]; }).join('; '); },
  set cookie(v) { var p = String(v).split(';')[0]; var i = p.indexOf('='); if (i > 0) cookieJar[p.slice(0, i).trim()] = p.slice(i + 1).trim(); }
};

function XMLHttpRequest() { this.readyState = 0; this.status = 0; this.responseText = ''; this._h = {}; }
XMLHttpRequest.prototype.open = function (m, u) { this._m = m; this._u = u; this.readyState = 1; };
XMLHttpRequest.prototype.setRequestHeader = function (k, v) { this._h[k] = v; };
XMLHttpRequest.prototype.getResponseHeader = function () { return null; };
XMLHttpRequest.prototype.getAllResponseHeaders = function () { return ''; };
XMLHttpRequest.prototype.abort = function () {};
XMLHttpRequest.prototype.send = function (body) {
  var x = this;
  __host.http(x._m, x._u, body == null ? '' : String(body), x._h, function (status, text) {
    x.status = status; x.responseText = text; x.response = text; x.readyState = 4;
    if (x.onreadystatechange) x.onreadystatechange();
    if (status >= 200 && status < 400) { if (x.onload) x.onload(); } else if (x.onerror) x.onerror();
  });
};
XMLHttpRequest.prototype.withCredentials = false;
window.XMLHttpRequest = XMLHttpRequest;
window.Image = function () { return new Element('img'); };
