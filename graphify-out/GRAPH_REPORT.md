# Graph Report - atoll  (2026-08-24)

## Corpus Check
- 77 files · ~67,330 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 1668 nodes · 3265 edges · 65 communities (63 shown, 2 thin omitted)
- Extraction: 88% EXTRACTED · 12% INFERRED · 0% AMBIGUOUS · INFERRED: 405 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `02c71b2d`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- ShellWindow
- LyricsService
- AiService
- ClaudeCliProvider
- AiToolbox
- MprisPlayer
- IpcService
- ShareSender
- ShareService
- Visualizer
- Application
- DBusMessageInfo
- Config
- MprisManager
- aiservice.cpp
- Battery
- ShareServer
- AnthropicProvider
- ShareDiscovery
- ImageStore
- NotificationModel
- shareserver.cpp
- AiProvider
- OsdMonitor
- shareservice.cpp
- mprisplayer.cpp
- NotificationData
- application.cpp
- Atoll
- credentialstore.cpp
- ScreenCapture
- setState
- CommandRunner
- Clock
- NotificationMonitor
- SharePeer
- geminiprovider.cpp
- AiToolCall
- PermissionBroker
- LockMonitor
- ShareIdentity
- main.cpp
- extractImage
- AiBackend
- AiTurn
- QObject
- osdmonitor.cpp
- GeminiProvider
- permissionbroker.cpp
- resolveArt
- notificationmodel.cpp
- shareserver.h
- ShareFile
- QTimer
- QString
- QObject
- QString
- AiRequest
- install.sh
- application.h
- sharecredentials.cpp
- atollctl
- Packaging
- shareservice.h
- offer

## God Nodes (most connected - your core abstractions)
1. `AiService` - 161 edges
2. `MprisPlayer` - 86 edges
3. `ShareService` - 81 edges
4. `Application` - 73 edges
5. `LyricsService` - 66 edges
6. `IpcService` - 55 edges
7. `ShareServer` - 47 edges
8. `ShellWindow` - 44 edges
9. `ShareSender` - 44 edges
10. `ClaudeCliProvider` - 43 edges

## Surprising Connections (you probably didn't know these)
- `reviewToolCall` --references--> `AiRequest`  [INFERRED]
  src/ai/aiservice.h → src/ai/aiprovider.h
- `testKey` --references--> `AiBackend`  [INFERRED]
  src/ai/aiservice.h → src/ai/aiprovider.h
- `AiService::AiService()` --calls--> `cliDetail`  [INFERRED]
  src/ai/aiservice.cpp → src/ai/aiservice.h
- `desktopGeometry()` --references--> `QScreen`  [INFERRED]
  src/ai/screencapture.cpp → src/app/shellwindow.h
- `outputs` --references--> `QScreen`  [INFERRED]
  src/ai/screencapture.h → src/app/shellwindow.h

## Import Cycles
- None detected.

## Communities (65 total, 2 thin omitted)

### Community 0 - "ShellWindow"
Cohesion: 0.10
Nodes (50): Layer, Config, Config, QObject, QQuickWindow, QSize, QString, QVariantList (+42 more)

### Community 1 - "LyricsService"
Cohesion: 0.06
Nodes (62): Config, Config, MprisManager, qint64, QObject, QString, QVariantList, Q_OBJECT (+54 more)

### Community 2 - "AiService"
Cohesion: 0.03
Nodes (72): PendingReview, QQueue, AiService, activityChanged, answerChanged, choiceChanged, cliChanged, configurationChanged (+64 more)

### Community 3 - "ClaudeCliProvider"
Cohesion: 0.06
Nodes (58): candidatePaths(), ClaudeCliProvider, abort, arguments, busy, ClaudeCliProvider::ClaudeCliProvider(), consume, describe (+50 more)

### Community 4 - "AiToolbox"
Cohesion: 0.09
Nodes (49): QJsonArray, AiToolbox, AiToolbox::AiToolbox(), cancel, clientAddendum, completed, definitions, execute (+41 more)

### Community 5 - "MprisPlayer"
Cohesion: 0.04
Nodes (38): Q_OBJECT, qint64, QObject, QTimer, MprisPlayer, artChanged, capabilitiesChanged, identityChanged (+30 more)

### Community 6 - "IpcService"
Cohesion: 0.08
Nodes (24): OpenReview, QDBusContext, QHash, QObject, QString, IpcService, askRequested, assistantRequested (+16 more)

### Community 7 - "ShareSender"
Cohesion: 0.07
Nodes (28): ShareCredentials, Q_OBJECT, QHash, qint64, QList, QObject, QPointer, QString (+20 more)

### Community 8 - "ShareService"
Cohesion: 0.05
Nodes (38): Q_OBJECT, QDateTime, qint64, QList, QObject, QString, QTimer, quint16 (+30 more)

### Community 9 - "Visualizer"
Cohesion: 0.07
Nodes (36): Config, Config, QList, QObject, Q_OBJECT, QByteArray, QList, QObject (+28 more)

### Community 10 - "Application"
Cohesion: 0.07
Nodes (27): Application, busTapChanged, islandRunning, m_ai, m_battery, m_busError, m_busTapActive, m_calendar (+19 more)

### Community 11 - "DBusMessageInfo"
Cohesion: 0.05
Nodes (38): DBusMessage, DBusMessageIter, DBusMonitor, Kind, QVariantMap, QObject, QStringList, DBusMessageInfo (+30 more)

### Community 12 - "Config"
Cohesion: 0.12
Nodes (31): QFileSystemWatcher, Config, applyWatch, changed, Config::Config(), deepMerge, defaults, defaultValue (+23 more)

### Community 13 - "MprisManager"
Cohesion: 0.11
Nodes (30): MprisPlayer, Config, Config, QObject, QString, QVariantList, Q_OBJECT, QList (+22 more)

### Community 14 - "aiservice.cpp"
Cohesion: 0.09
Nodes (39): abandonChoice, allowEverything, askChoice, askUserFor, bringToFront, busy, captureScreenFor, choose (+31 more)

### Community 15 - "Battery"
Cohesion: 0.09
Nodes (25): Battery, apply, Battery::Battery(), changed, iconName, m_iconName, m_percent, m_present (+17 more)

### Community 16 - "ShareServer"
Cohesion: 0.06
Nodes (63): qintptr, QTcpServer, Session, QString, ShareCredentials, mint(), read(), ShareCredentials::load() (+55 more)

### Community 17 - "AnthropicProvider"
Cohesion: 0.21
Nodes (16): AnthropicProvider::AnthropicProvider(), buildBody, buildRequest, handleEvent, resetTurn, sealBlock, Block, QByteArray (+8 more)

### Community 18 - "ShareDiscovery"
Cohesion: 0.10
Nodes (30): QByteArray, QHostAddress, QList, QObject, quint16, Q_OBJECT, QObject, QSet (+22 more)

### Community 19 - "ImageStore"
Cohesion: 0.12
Nodes (23): QMutex, QPixmap, QQuickImageProvider, QColor, QImage, QSize, QString, QHash (+15 more)

### Community 20 - "NotificationModel"
Cohesion: 0.09
Nodes (24): QAbstractListModel, QModelIndex, Config, QByteArray, QHash, QVariant, QVariantMap, Q_OBJECT (+16 more)

### Community 21 - "shareserver.cpp"
Cohesion: 0.08
Nodes (41): QDate, CalendarService, applyIntervals, buildPool, CalendarService::CalendarService(), changed, computeToday, computeUpcoming (+33 more)

### Community 22 - "AiProvider"
Cohesion: 0.11
Nodes (21): AiProvider, abort, AiProvider::AiProvider(), buildBody, buildRequest, busy, consume, describeHttpError (+13 more)

### Community 23 - "OsdMonitor"
Cohesion: 0.10
Nodes (16): Config, DBusMessageInfo, Q_OBJECT, QObject, QString, OsdMonitor, dismissed, m_config (+8 more)

### Community 24 - "shareservice.cpp"
Cohesion: 0.18
Nodes (22): Config, QObject, QString, QVariantList, defaultAlias(), configure, dataDirectory, destination (+14 more)

### Community 25 - "mprisplayer.cpp"
Cohesion: 0.17
Nodes (21): qint64, QObject, QString, QVariantList, call, fetchAll, iconName, MprisPlayer::MprisPlayer() (+13 more)

### Community 26 - "NotificationData"
Cohesion: 0.09
Nodes (22): QColor, QDateTime, QString, QStringList, NotificationData, accent, actions, appIcon (+14 more)

### Community 27 - "application.cpp"
Cohesion: 0.16
Nodes (18): QUrl, activateApp, adjustVolume, Application::Application(), copyText, debugState, debugSurface, instance (+10 more)

### Community 28 - "Atoll"
Cohesion: 0.08
Nodes (25): Arch, in one go, Atoll, By hand, Calendar, Configuring, Connecting it, Controlling it, How it gets its information (+17 more)

### Community 29 - "credentialstore.cpp"
Cohesion: 0.26
Nodes (19): QObject, QString, CredentialStore, backendFor, CredentialStore::CredentialStore(), environmentVariable, filePath, fromFile (+11 more)

### Community 30 - "ScreenCapture"
Cohesion: 0.10
Nodes (41): qreal, QRect, QRectF, QObject, QString, QVariantList, QVariantMap, uint (+33 more)

### Community 31 - "setState"
Cohesion: 0.16
Nodes (27): addStep, advanceQueue, AiService::AiService(), allow, answerReview, cancel, deny, executeNow (+19 more)

### Community 32 - "CommandRunner"
Cohesion: 0.15
Nodes (17): Job, Q_SIGNALS, CommandRunner, cancelAll, collect, CommandRunner::CommandRunner(), condense, finished (+9 more)

### Community 33 - "Clock"
Cohesion: 0.12
Nodes (17): Clock, Clock::Clock(), date, m_config, m_timer, QML_ELEMENT, scheduleNext, tick (+9 more)

### Community 34 - "NotificationMonitor"
Cohesion: 0.12
Nodes (16): PendingCall, Config, Q_OBJECT, QHash, QObject, quint32, quint64, NotificationMonitor (+8 more)

### Community 35 - "SharePeer"
Cohesion: 0.12
Nodes (17): answer, remember, QDateTime, QHostAddress, QString, quint16, QVariantMap, shareAddressString() (+9 more)

### Community 36 - "geminiprovider.cpp"
Cohesion: 0.10
Nodes (26): QByteArray, QJsonObject, QJsonValue, QNetworkAccessManager, QNetworkRequest, QObject, QString, GeminiProvider (+18 more)

### Community 38 - "PermissionBroker"
Cohesion: 0.06
Nodes (51): aiRiskName(), AiToolResult, content, id, image, imageMediaType, isError, AiVerdict (+43 more)

### Community 39 - "LockMonitor"
Cohesion: 0.18
Nodes (12): QML_UNCREATABLE, QObject, Q_OBJECT, QObject, LockMonitor, lockedChanged, LockMonitor::LockMonitor(), m_locked (+4 more)

### Community 40 - "ShareIdentity"
Cohesion: 0.15
Nodes (12): QHostAddress, QUdpSocket, setIdentity, setIdentity, QJsonObject, ShareIdentity, alias, deviceModel (+4 more)

### Community 41 - "main.cpp"
Cohesion: 0.20
Nodes (11): QQmlApplicationEngine, QString, reply(), runPermissionHook(), QQuickWindow, QString, firstWindow(), main() (+3 more)

### Community 42 - "extractImage"
Cohesion: 0.13
Nodes (16): QImage, Config, DBusMessageInfo, QColor, QImage, QObject, QString, QStringList (+8 more)

### Community 43 - "AiBackend"
Cohesion: 0.14
Nodes (12): AiBackend, abort, busy, defaultModel, failed, id, textDelta, thoughtDelta (+4 more)

### Community 44 - "AiTurn"
Cohesion: 0.15
Nodes (11): AiTurn, image, imageMediaType, rawContent, rawProvider, role, text, toolCalls (+3 more)

### Community 45 - "QObject"
Cohesion: 0.13
Nodes (20): QString, LockscreenOverlay, allow, available, m_overlay, m_reason, m_resolved, resolve (+12 more)

### Community 46 - "osdmonitor.cpp"
Cohesion: 0.29
Nodes (11): send, activeBackend, ask, configured, engage, finishToolRound, model, provider (+3 more)

### Community 47 - "GeminiProvider"
Cohesion: 0.29
Nodes (14): QNetworkReply, QList, QNetworkRequest, QObject, QString, abandon, cancel, currentFile (+6 more)

### Community 48 - "permissionbroker.cpp"
Cohesion: 0.16
Nodes (12): AnthropicProvider, m_blocks, m_calls, m_raw, m_stopReason, public, Block, Q_OBJECT (+4 more)

### Community 49 - "resolveArt"
Cohesion: 0.19
Nodes (13): QNetworkAccessManager, QStringList, QVariant, QVariantMap, demarshall(), applyMetadata, applyPlayerProperties, applyRootProperties (+5 more)

### Community 50 - "notificationmodel.cpp"
Cohesion: 0.29
Nodes (12): Config, QObject, QString, quint32, quint64, close, dismiss, indexOfUid (+4 more)

### Community 51 - "shareserver.h"
Cohesion: 0.23
Nodes (13): start, Config, DBusMessageInfo, QObject, QString, QStringList, emitEvent, handleMessage (+5 more)

### Community 52 - "ShareFile"
Cohesion: 0.29
Nodes (7): qint64, ShareFile, id, mimeType, name, path, size

### Community 53 - "QTimer"
Cohesion: 0.14
Nodes (13): QObject, assistant, collapse, dismiss, expand, IpcService::IpcService(), quit, raise (+5 more)

### Community 55 - "QObject"
Cohesion: 0.19
Nodes (9): QList, QObject, QPointer, QNetworkAccessManager, QNetworkReply, QProcess, QFile, QNetworkAccessManager (+1 more)

### Community 56 - "QString"
Cohesion: 0.27
Nodes (10): QString, answerScreenCapture, answerToolReview, answerUserChoice, ask, listScreens, sendReply, showProgress (+2 more)

### Community 57 - "AiRequest"
Cohesion: 0.22
Nodes (9): AiRequest, baseUrl, effort, history, maxTokens, model, systemPrompt, tools (+1 more)

### Community 58 - "install.sh"
Cohesion: 0.52
Nodes (6): ask(), die(), note(), say(), install.sh script, warn()

### Community 59 - "application.h"
Cohesion: 0.29
Nodes (6): create, DBusMonitor, Config, NotificationMonitor, QJSEngine, QQmlEngine

### Community 60 - "sharecredentials.cpp"
Cohesion: 0.33
Nodes (6): QStringList, askUser, captureScreen, park, reviewToolCall, share

### Community 61 - "atollctl"
Cohesion: 0.83
Nodes (3): atollctl script, call(), usage()

### Community 62 - "Packaging"
Cohesion: 0.50
Nodes (3): Packaging, Publishing to the AUR, The assistant's client

### Community 63 - "shareservice.h"
Cohesion: 0.33
Nodes (5): Config, QNetworkAccessManager, ShareDiscovery, ShareSender, ShareServer

### Community 64 - "offer"
Cohesion: 0.50
Nodes (5): QList, QStringList, collect, offer, offerPaths

## Knowledge Gaps
- **499 isolated node(s):** `role`, `text`, `toolCalls`, `toolResults`, `image` (+494 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **2 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `AiService` connect `AiService` to `PermissionBroker`, `LockMonitor`, `application.h`, `Application`, `AiTurn`, `osdmonitor.cpp`, `aiservice.cpp`, `application.cpp`, `setState`?**
  _High betweenness centrality (0.199) - this node is a cross-community bridge._
- **Why does `QML_UNCREATABLE` connect `LockMonitor` to `ShellWindow`, `LyricsService`, `AiService`, `Clock`, `MprisPlayer`, `IpcService`, `ShareService`, `Visualizer`, `Config`, `MprisManager`, `Battery`, `shareserver.cpp`, `OsdMonitor`?**
  _High betweenness centrality (0.133) - this node is a cross-community bridge._
- **Why does `ShareService` connect `ShareService` to `offer`, `SharePeer`, `LockMonitor`, `application.cpp`, `ShareIdentity`, `Application`, `ShareFile`, `shareservice.cpp`, `application.h`, `ScreenCapture`, `shareservice.h`?**
  _High betweenness centrality (0.126) - this node is a cross-community bridge._
- **What connects `role`, `text`, `toolCalls` to the rest of the system?**
  _499 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `ShellWindow` be split into smaller, more focused modules?**
  _Cohesion score 0.09579100145137881 - nodes in this community are weakly interconnected._
- **Should `LyricsService` be split into smaller, more focused modules?**
  _Cohesion score 0.05672926447574335 - nodes in this community are weakly interconnected._
- **Should `AiService` be split into smaller, more focused modules?**
  _Cohesion score 0.02821316614420063 - nodes in this community are weakly interconnected._