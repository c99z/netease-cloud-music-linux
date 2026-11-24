const crypto = require('node:crypto')
const fs = require('node:fs')
const http = require('node:http')
const path = require('node:path')

const [, , repository, stateDirectory, configurationDirectory] = process.argv
const clientKey = process.env.RECORD_ROOM_CLIENT_KEY
const routes = new Map([
  ['/personalized', ['personalized', ['limit']]],
  ['/personalized/newsong', ['personalized_newsong', ['limit']]],
  ['/top/playlist', ['top_playlist', ['limit', 'cat', 'offset']]],
  ['/playlist/catlist', ['playlist_catlist', []]],
  ['/toplist/detail', ['toplist_detail', []]],
  ['/toplist/groups', ['toplist_detail_v2', []]],
  ['/banner', ['banner', ['type']]],
  ['/playlist/detail', ['playlist_detail', ['id']]],
  ['/playlist/comments', ['comment_playlist', ['id', 'limit', 'offset']]],
  ['/playlist/subscribers', ['playlist_subscribers', ['id', 'limit', 'offset']]],
  ['/search/suggest', ['search_suggest', ['keywords']]],
  ['/search/hot', ['search_hot_detail', []]],
  ['/simi/song', ['simi_song', ['id']]],
  ['/simi/playlist', ['simi_playlist', ['id']]],
  ['/artist/detail', ['artist_detail', ['id']]],
  ['/artist/albums', ['artist_album', ['id', 'limit']]],
  ['/artist/similar', ['simi_artist', ['id']]],
  ['/artist/desc', ['artist_desc', ['id']]],
  ['/album/newest', ['album_newest', []]],
  ['/album', ['album', ['id']]],
  ['/top/song', ['top_song', ['type']]],
  ['/top/album', ['top_album', ['area', 'limit']]],
  ['/artist/songs', ['artist_songs', ['id', 'limit']]],
  ['/artists', ['artist_list', ['area', 'type', 'limit', 'initial', 'offset']]],
  ['/style/song', ['style_song', ['tagId', 'size']]],
  ['/cloudsearch', ['cloudsearch', ['keywords', 'type', 'limit', 'offset']]],
  ['/playlist/track/all', ['playlist_track_all', ['id', 'limit', 'offset']]],
  ['/song/url', ['song_url', ['id', 'br']]],
  ['/song/detail', ['song_detail', ['ids']]],
  ['/lyric', ['lyric', ['id']]],
  ['/dj/recommend', ['dj_recommend', []]],
  ['/dj/program/recommend', ['program_recommend', ['limit']]],
  ['/dj/program/detail', ['dj_program_detail', ['id']]],
  ['/dj/categories', ['dj_catelist', []]],
  ['/dj/by-category', ['dj_recommend_type', ['type']]],
  ['/dj/toplist', ['dj_toplist', ['type', 'limit']]],
  ['/voice/list', ['voicelist_list', ['voiceListId', 'limit']]],
  ['/dj/program', ['dj_program', ['rid', 'limit', 'offset']]],
  ['/account/playlists', ['user_playlist', ['limit'], true]],
  ['/account/daily', ['recommend_songs', [], true]],
  ['/account/recommendations', ['recommend_resource', [], true]],
  ['/account/recent', ['record_recent_song', ['limit'], true]],
  ['/account/recent/playlists', ['record_recent_playlist', ['limit'], true]],
  ['/account/recent/albums', ['record_recent_album', ['limit'], true]],
  ['/account/recent/radios', ['record_recent_dj', ['limit'], true]],
  ['/account/recent/voices', ['record_recent_voice', ['limit'], true]],
  ['/account/recent/videos', ['record_recent_video', ['limit'], true]],
  ['/account/created-voices', ['voicelist_my_created', ['limit'], true]],
  ['/account/collections/albums', ['album_sublist', ['limit'], true]],
  ['/account/collections/artists', ['artist_sublist', ['limit'], true]],
  ['/account/collections/videos', ['mv_sublist', ['limit'], true]],
  ['/account/events', ['event', ['pagesize'], true]],
  ['/account/likes', ['likelist', [], true]],
  ['/account/radios', ['dj_sublist', ['limit'], true]],
  ['/account/follows', ['user_follows', ['limit'], true]],
  ['/account/fm', ['personal_fm', [], true]],
  ['/account/recent-listen', ['recent_listen_list', [], true]],
])

function send(response, status, body) {
  response.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Cache-Control': 'no-store',
  })
  response.end(JSON.stringify(body))
}

function cleanCookies(cookies) {
  const clean = {}
  for (const name of ['MUSIC_U', 'MUSIC_R_T', '__csrf', 'NMTID']) {
    const value = cookies?.[name]
    if (typeof value === 'string' && /^[^\x00-\x20;]+$/.test(value) && value.length <= 8192)
      clean[name] = value
  }
  return clean
}

function cookiesFromHeaders(headers) {
  const cookies = {}
  for (const line of Array.isArray(headers) ? headers : []) {
    const pair = line.split(';', 1)[0]
    const separator = pair.indexOf('=')
    if (separator > 0)
      cookies[pair.slice(0, separator).trim()] = pair.slice(separator + 1).trim()
  }
  return cleanCookies(cookies)
}

function start() {
  if (!clientKey || !configurationDirectory)
    throw new Error('The desktop client must initialize the local API.')
  fs.mkdirSync(stateDirectory, { recursive: true, mode: 0o700 })
  fs.mkdirSync(configurationDirectory, { recursive: true, mode: 0o700 })
  process.env.TMPDIR = stateDirectory
  process.env.NETEASE_COOKIE = ''
  process.env.ENABLE_GENERAL_UNBLOCK = 'false'
  process.env.ENABLE_RANDOM_CN_IP = 'false'
  process.env.ENABLE_PROXY = 'false'
  process.env.DOTENV_CONFIG_QUIET = 'true'

  try {
    fs.writeFileSync(path.join(stateDirectory, 'anonymous_token'), '', { flag: 'wx', mode: 0o600 })
  } catch (error) {
    if (error.code !== 'EEXIST') throw error
  }

  const api = require(path.join(repository, 'main.js'))
  const publishedReadRoutes = new Map([
    ['/scene/tags', ['/api/playlist/tags', {}]],
    ['/account/events', ['/api/event/pc/history/feed/get', { cursor: '-1', limit: 25 }, true]],
    ['/account/podcasts/saved', ['/api/social/my/subscribed/voicelist/v1', { limit: 50 }, true]],
    ['/account/podcasts/bought', ['/api/voicelist/bought/list', { limit: 50 }, true]],
    ['/account/podcasts/liked', ['/api/content/my/liked/voice', { limit: 50 }, true]],
    ['/vip/playlists', ['/api/vipnewcenter/app/viptab/recommend/playList/list', { limit: 30 }]],
    ['/vip/charts', ['/api/vipnewcenter/app/viptab/ranking/list', {}]],
    ['/account/vip/songs', ['/api/vipnewcenter/app/viptab/recommend/song/list', { limit: 30 }, true]],
  ])
  async function essentialSection(route) {
    const upstream = require(path.join(repository, 'util', 'request.js'))
    const options = { cookie, crypto: 'weapi', timeout: 8500 }
    const official = () => upstream('/api/playlist/category/list',
      { cat: '官方', limit: route === '/essential/official' ? 40 : 6, newStyle: true }, options)
    const podcasts = () => upstream('/api/pc/voicelist/rcmd/list', { limit: 12 }, options)
    if (route === '/essential/official') return official()
    if (route === '/essential/podcasts') return podcasts()

    const parts = await Promise.allSettled([
      official(), api.personalized_newsong({ limit: 12, areaId: 0, cookie, timeout: 8500 }), podcasts(),
    ])
    const bodies = parts.map(result => result.status === 'fulfilled' ? result.value.body : {})
    const [playlists, music, voices] = bodies
    const overview = {
      official: playlists.playlists || [],
      latest: music.result || [],
      podcasts: voices.data?.recommendVoiceVOS || [],
    }
    const loaded = Object.values(overview).some(items => items.length)
    return { status: loaded ? 200 : 502, body: { code: loaded ? 200 : 502, ...overview } }
  }
  const webHomeBlocks = [
    'HOMPAGE_BLOCK_VIP_RCMD', 'HOMEPAGE_BLOCK_RED_SIMILAR_SONG',
    'HOMEPAGE_BLOCK_STYLE_RCMD', 'HOMEPAGE_MUSIC_PODCAST_RCMD_BLOCK',
    'HOMEPAGE_BLOCK_PLAYLIST_RCMD', 'HOMEPAGE_BLOCK_ALL_LISTEN',
    'HOMEPAGE_BLOCK_OFFICIAL_PLAYLIST',
  ]

  async function desktopHome(route) {
    const request = require(path.join(repository, 'util', 'request.js'))
    const options = { cookie, crypto: 'weapi', timeout: 10000 }
    if (route === '/homepage/radar')
      return request('/api/pc/customize/block/page', { newStyle: true }, options)
    return request('/api/homepage/block/page', {
      cursor: JSON.stringify({ offset: 0, blockCodeOrderList: webHomeBlocks }),
      extInfo: JSON.stringify({ abInfo: { 'hp-new-homepageV3.1': 't3' } }),
      newStyle: true,
    }, options)
  }
  const sessionFile = path.join(configurationDirectory, 'session.json')
  let cookie = {}
  let profile = null
  let qrKey = ''
  let loginRevision = 0
  try {
    cookie = cleanCookies(JSON.parse(fs.readFileSync(sessionFile, 'utf8')))
  } catch (error) {
    if (error.code !== 'ENOENT' && !(error instanceof SyntaxError))
      throw error
  }

  function saveSession(next) {
    if (next.MUSIC_U) {
      const temporary = path.join(configurationDirectory, crypto.randomUUID() + '.tmp')
      fs.writeFileSync(temporary, JSON.stringify(next), { flag: 'wx', mode: 0o600 })
      fs.renameSync(temporary, sessionFile)
    } else {
      try {
        fs.unlinkSync(sessionFile)
      } catch (error) {
        if (error.code !== 'ENOENT') throw error
      }
    }
    cookie = next
    profile = null
  }

  async function currentProfile() {
    if (!cookie.MUSIC_U) return null
    if (profile) return profile
    const session = cookie
    const result = await api.user_account({ cookie: session, timeout: 8500 })
    const p = result.body?.profile ?? result.body?.data?.profile
    if (session !== cookie) return null
    if (result.body?.code !== 200) throw new Error('Account lookup failed')
    if (!p?.userId) {
      saveSession({})
      return null
    }
    profile = {
      userId: p.userId,
      nickname: p.nickname || '网易云用户',
      avatarUrl: p.avatarUrl || '',
    }
    return profile
  }

  async function accountRequest(route, method, parameters) {
    if (route === '/account/session' && method === 'GET')
      return { code: 200, profile: await currentProfile() }

    if (route === '/account/logout' && method === 'POST') {
      ++loginRevision
      qrKey = ''
      saveSession({})
      return { code: 200 }
    }

    if (route === '/account/qr' && method === 'GET') {
      const revision = ++loginRevision
      qrKey = ''
      const result = await api.login_qr_key({ timeout: 8500 })
      const key = result.body?.data?.unikey
      if (!key || revision !== loginRevision) throw new Error('QR code request failed')
      const qr = await api.login_qr_create({ key, qrimg: true })
      if (revision !== loginRevision) throw new Error('QR code request superseded')
      qrKey = key
      return { code: 200, image: qr.body.data.qrimg }
    }

    if (route === '/account/qr/check' && method === 'GET') {
      if (!qrKey) return { code: 200, phase: 800 }
      const revision = loginRevision
      const result = await api.login_qr_check({ key: qrKey, timeout: 8500 })
      if (revision !== loginRevision) return { code: 200, phase: 800 }
      const phase = Number(result.body?.code)
      if (![800, 801, 802, 803].includes(phase)) throw new Error('QR code check failed')
      if (phase === 803) {
        const signin = cookiesFromHeaders(result.cookie)
        if (!signin.MUSIC_U) throw new Error('Missing session cookie')
        saveSession(signin)
        qrKey = ''
      }
      if (phase === 800) qrKey = ''
      return { code: 200, phase }
    }

    if ((route === '/account/like' || route === '/account/fm/trash') && method === 'POST') {
      if (!(await currentProfile())) return { code: 401 }
      const id = parameters.get('id')
      if (!/^\d+$/.test(id || '')) return { code: 400 }
      const result = route === '/account/like'
        ? await api.like({ id, like: parameters.get('like') === 'false' ? 'false' : 'true', cookie, timeout: 8500 })
        : await api.fm_trash({ id, cookie, timeout: 8500 })
      return { code: result.body?.code || 502 }
    }

    if (route === '/account/playlist/subscribe' && method === 'POST') {
      if (!(await currentProfile())) return { code: 401 }
      const id = parameters.get('id')
      if (!/^\d+$/.test(id || '')) return { code: 400 }
      const result = await api.playlist_subscribe({
        id, t: parameters.get('subscribe') === 'false' ? 2 : 1, cookie, timeout: 8500,
      })
      return { code: result.body?.code || 502 }
    }

    if (route === '/account/playlist/tracks' && method === 'POST') {
      if (!(await currentProfile())) return { code: 401 }
      const pid = parameters.get('pid')
      const track = parameters.get('track')
      const op = parameters.get('op')
      if (!/^\d+$/.test(pid || '') || !/^\d+$/.test(track || '') || !['add', 'del'].includes(op))
        return { code: 400 }
      const result = await api.playlist_tracks({ op, pid, tracks: track, cookie, timeout: 8500 })
      const code = result.body?.code ?? result.body?.body?.code
      return { code: code || 502, message: result.body?.message || result.body?.body?.message || '' }
    }

    if (route === '/account/playlists/create' && method === 'POST') {
      if (!(await currentProfile())) return { code: 401 }
      const name = parameters.get('name')?.trim()
      if (!name || name.length > 40) return { code: 400 }
      const result = await api.playlist_create({ name, cookie, timeout: 8500 })
      return { code: result.body?.code || 502 }
    }
    return null
  }

  const server = http.createServer(async (request, response) => {
    if (request.headers['x-record-room-key'] !== clientKey ||
        request.headers.origin || request.headers['sec-fetch-site']) {
      send(response, 403, { code: 403 })
      return
    }
    const baseUrl = 'http://127.0.0.1'
    if (!request.url.startsWith('/') || !URL.canParse(request.url, baseUrl)) {
      send(response, 400, { code: 400 })
      return
    }
    const url = new URL(request.url, baseUrl)
    if (process.env.RECORD_ROOM_TRACE) console.error('[req] ' + url.pathname)
    const route = routes.get(url.pathname)
    try {
      const account = await accountRequest(url.pathname, request.method, url.searchParams)
      if (account) {
        send(response, 200, account)
        return
      }
      if ((url.pathname === '/homepage/blocks' || url.pathname === '/homepage/radar') && request.method === 'GET') {
        const result = await desktopHome(url.pathname)
        send(response, result.status, result.body)
        return
      }
      if (url.pathname === '/scene/playlists' && request.method === 'GET') {
        const catId = url.searchParams.get('id')
        if (!/^\d+$/.test(catId || '')) {
          send(response, 400, { code: 400 })
          return
        }
        const upstream = require(path.join(repository, 'util', 'request.js'))
        const result = await upstream('/api/playlist/category/list',
          { catId, limit: 20, newStyle: true }, { cookie, crypto: 'weapi', timeout: 8500 })
        send(response, result.status, result.body)
        return
      }
      if (['/essential/overview', '/essential/official', '/essential/podcasts'].includes(url.pathname)
          && request.method === 'GET') {
        const result = await essentialSection(url.pathname)
        send(response, result.status, result.body)
        return
      }
      const published = publishedReadRoutes.get(url.pathname)
      if (published && request.method === 'GET') {
        const [uri, data, requiresAccount] = published
        if (requiresAccount && !(await currentProfile())) {
          send(response, 401, { code: 401 })
          return
        }
        const upstream = require(path.join(repository, 'util', 'request.js'))
        try {
          const result = await upstream(uri, data, { cookie, crypto: 'weapi', timeout: 8500 })
          send(response, result.status, result.body)
        } catch (error) {
          if (url.pathname !== '/account/events') throw error
          const result = await api.event({ cookie, pagesize: 25, timeout: 8500 })
          send(response, result.status, result.body)
        }
        return
      }
      if (!route || request.method !== 'GET') {
        send(response, 404, { code: 404 })
        return
      }
      const [method, keys, requiresAccount] = route
      const query = Object.fromEntries(
        keys.flatMap((key) => url.searchParams.has(key) ? [[key, url.searchParams.get(key)]] : [])
      )
      if (requiresAccount) {
        const account = await currentProfile()
        if (!account) {
          send(response, 401, { code: 401 })
          return
        }
        query.uid = String(account.userId)
      }
      const result = await api[method]({ ...query, cookie, timeout: 10000 })
      const { cookie: unusedCookie, ...body } = result.body || {}
      send(response, result.status, body)
    } catch (error) {
      const status = Number.isInteger(error.status) && error.status >= 400 && error.status <= 599
        ? error.status : 502
      send(response, status, { code: status })
    }
  })

  server.on('error', (error) => {
    console.error(error.message)
    process.exitCode = 1
  })
  server.listen(0, '127.0.0.1', () => {
    console.log('QT_NCM_API_PORT=' + server.address().port)
  })
}

try {
  start()
} catch (error) {
  console.error(error.message)
  process.exitCode = 1
}
