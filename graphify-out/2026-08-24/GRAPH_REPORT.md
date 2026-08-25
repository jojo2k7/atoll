# Graph Report - atoll  (2026-08-24)

## Corpus Check
- 75 files · ~62,198 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 1616 nodes · 3156 edges · 60 communities (57 shown, 3 thin omitted)
- Extraction: 88% EXTRACTED · 12% INFERRED · 0% AMBIGUOUS · INFERRED: 392 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `bc9221a1`
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
- AiRequest
- install.sh
- sharecredentials.cpp
- atollctl
- Packaging

## God Nodes (most connected - your core abstractions)
1. `AiService` - 161 edges
2. `MprisPlayer` - 86 edges
3. `ShareService` - 81 edges
4. `Application` - 70 edges
5. `LyricsService` - 66 edges
6. `IpcService` - 53 edges
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

## Communities (60 total, 3 thin omitted)

### Community 0 - "ShellWindow"
Cohesion: 0.07
Nodes (59): Layer, QDBusContext, QTcpServer, QObject, QPointer, QVariantList, QHash, QProcess (+51 more)

### Community 1 - "LyricsService"
Cohesion: 0.06
Nodes (62): Config, Config, MprisManager, qint64, QObject, QString, QVariantList, Q_OBJECT (+54 more)

### Community 2 - "AiService"
Cohesion: 0.03
Nodes (71): PendingReview, QQueue, AiService, activityChanged, answerChanged, choiceChanged, cliChanged, configurationChanged (+63 more)

### Community 3 - "ClaudeCliProvider"
Cohesion: 0.06
Nodes (57): candidatePaths(), ClaudeCliProvider, abort, arguments, busy, ClaudeCliProvider::ClaudeCliProvider(), consume, describe (+49 more)

### Community 4 - "AiToolbox"
Cohesion: 0.09
Nodes (49): QJsonArray, AiToolbox, AiToolbox::AiToolbox(), cancel, clientAddendum, completed, definitions, execute (+41 more)

### Community 5 - "MprisPlayer"
Cohesion: 0.04
Nodes (38): Q_OBJECT, qint64, QObject, QTimer, MprisPlayer, artChanged, capabilitiesChanged, identityChanged (+30 more)

### Community 6 - "IpcService"
Cohesion: 0.07
Nodes (50): OpenReview, QObject, QString, QStringList, QHash, QObject, QString, IpcService (+42 more)

### Community 7 - "ShareSender"
Cohesion: 0.06
Nodes (46): QNetworkReply, QList, QList, QNetworkRequest, QObject, QString, ShareCredentials, Q_OBJECT (+38 more)

### Community 8 - "ShareService"
Cohesion: 0.05
Nodes (42): Config, Q_OBJECT, QDateTime, qint64, QList, QObject, QTimer, quint16 (+34 more)

### Community 9 - "Visualizer"
Cohesion: 0.07
Nodes (36): Config, Config, QList, QObject, Q_OBJECT, QByteArray, QList, QObject (+28 more)

### Community 10 - "Application"
Cohesion: 0.06
Nodes (30): Application, busTapChanged, create, m_ai, m_battery, m_busError, m_busTapActive, m_clock (+22 more)

### Community 11 - "DBusMessageInfo"
Cohesion: 0.05
Nodes (38): DBusMessage, DBusMessageIter, DBusMonitor, Kind, QVariantMap, QObject, QStringList, DBusMessageInfo (+30 more)

### Community 12 - "Config"
Cohesion: 0.12
Nodes (30): QFileSystemWatcher, Config, applyWatch, changed, Config::Config(), deepMerge, defaults, defaultValue (+22 more)

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
Cohesion: 0.07
Nodes (28): quint16, ShareCredentials, Connection, Q_OBJECT, QHash, QPointer, QString, QTimer (+20 more)

### Community 17 - "AnthropicProvider"
Cohesion: 0.11
Nodes (27): AnthropicProvider, AnthropicProvider::AnthropicProvider(), buildBody, buildRequest, handleEvent, m_blocks, m_calls, m_raw (+19 more)

### Community 18 - "ShareDiscovery"
Cohesion: 0.10
Nodes (29): QByteArray, QHostAddress, QList, QObject, quint16, Q_OBJECT, QObject, QSet (+21 more)

### Community 19 - "ImageStore"
Cohesion: 0.12
Nodes (23): QMutex, QPixmap, QQuickImageProvider, QColor, QImage, QSize, QString, QHash (+15 more)

### Community 20 - "NotificationModel"
Cohesion: 0.11
Nodes (21): QAbstractListModel, QModelIndex, Config, QVariant, QVariantMap, Q_OBJECT, QList, NotificationModel (+13 more)

### Community 21 - "shareserver.cpp"
Cohesion: 0.21
Nodes (23): Session, Connection, QByteArray, QObject, QString, reason(), sanitise(), abortSession (+15 more)

### Community 22 - "AiProvider"
Cohesion: 0.10
Nodes (24): AiProvider, abort, AiProvider::AiProvider(), buildBody, buildRequest, busy, consume, describeHttpError (+16 more)

### Community 23 - "OsdMonitor"
Cohesion: 0.09
Nodes (26): Config, Config, DBusMessageInfo, QObject, QString, DBusMessageInfo, Q_OBJECT, QObject (+18 more)

### Community 24 - "shareservice.cpp"
Cohesion: 0.16
Nodes (24): qreal, Config, QObject, QString, QVariantList, defaultAlias(), configure, dataDirectory (+16 more)

### Community 25 - "mprisplayer.cpp"
Cohesion: 0.16
Nodes (23): qint64, QNetworkAccessManager, QObject, QString, QVariantList, call, fetchAll, iconName (+15 more)

### Community 26 - "NotificationData"
Cohesion: 0.09
Nodes (22): QColor, QDateTime, QString, QStringList, NotificationData, accent, actions, appIcon (+14 more)

### Community 27 - "application.cpp"
Cohesion: 0.14
Nodes (20): QUrl, activateApp, adjustVolume, Application::Application(), copyText, debugState, debugSurface, instance (+12 more)

### Community 28 - "Atoll"
Cohesion: 0.08
Nodes (23): Arch, in one go, Atoll, By hand, Configuring, Connecting it, Controlling it, How it gets its information, Installing (+15 more)

### Community 29 - "credentialstore.cpp"
Cohesion: 0.26
Nodes (19): QObject, QString, CredentialStore, backendFor, CredentialStore::CredentialStore(), environmentVariable, filePath, fromFile (+11 more)

### Community 30 - "ScreenCapture"
Cohesion: 0.10
Nodes (39): QRect, QRectF, QObject, QString, QVariantList, QVariantMap, uint, desktopGeometry() (+31 more)

### Community 31 - "setState"
Cohesion: 0.16
Nodes (27): addStep, advanceQueue, AiService::AiService(), allow, answerReview, cancel, deny, executeNow (+19 more)

### Community 32 - "CommandRunner"
Cohesion: 0.15
Nodes (17): Job, Q_SIGNALS, CommandRunner, cancelAll, collect, CommandRunner::CommandRunner(), condense, finished (+9 more)

### Community 33 - "Clock"
Cohesion: 0.12
Nodes (18): QTimer, Clock, Clock::Clock(), date, m_config, m_timer, QML_ELEMENT, scheduleNext (+10 more)

### Community 34 - "NotificationMonitor"
Cohesion: 0.11
Nodes (18): PendingCall, Config, QStringList, Q_OBJECT, QHash, QObject, quint32, quint64 (+10 more)

### Community 35 - "SharePeer"
Cohesion: 0.14
Nodes (13): answer, remember, QDateTime, QVariantMap, SharePeer, address, alias, deviceModel (+5 more)

### Community 36 - "geminiprovider.cpp"
Cohesion: 0.24
Nodes (12): QByteArray, QJsonObject, QJsonValue, QNetworkRequest, QString, buildBody, buildRequest, handleEvent (+4 more)

### Community 38 - "PermissionBroker"
Cohesion: 0.06
Nodes (52): aiRiskName(), AiToolResult, content, id, image, imageMediaType, isError, AiVerdict (+44 more)

### Community 39 - "LockMonitor"
Cohesion: 0.18
Nodes (12): QML_UNCREATABLE, QObject, Q_OBJECT, QObject, LockMonitor, lockedChanged, LockMonitor::LockMonitor(), m_locked (+4 more)

### Community 40 - "ShareIdentity"
Cohesion: 0.12
Nodes (17): QHostAddress, QUdpSocket, setIdentity, setIdentity, setIdentity, QHostAddress, QJsonObject, QString (+9 more)

### Community 41 - "main.cpp"
Cohesion: 0.16
Nodes (14): QQmlApplicationEngine, QString, reply(), runPermissionHook(), start, QStringList, matchRules, QQuickWindow (+6 more)

### Community 42 - "extractImage"
Cohesion: 0.15
Nodes (14): QImage, Config, DBusMessageInfo, QColor, QImage, QObject, QString, quint64 (+6 more)

### Community 43 - "AiBackend"
Cohesion: 0.14
Nodes (12): AiBackend, abort, busy, defaultModel, failed, id, textDelta, thoughtDelta (+4 more)

### Community 44 - "AiTurn"
Cohesion: 0.17
Nodes (10): AiTurn, image, imageMediaType, rawContent, rawProvider, role, text, toolCalls (+2 more)

### Community 45 - "QObject"
Cohesion: 0.13
Nodes (20): QString, LockscreenOverlay, allow, available, m_overlay, m_reason, m_resolved, resolve (+12 more)

### Community 46 - "osdmonitor.cpp"
Cohesion: 0.29
Nodes (11): send, activeBackend, ask, configured, engage, finishToolRound, model, provider (+3 more)

### Community 47 - "GeminiProvider"
Cohesion: 0.18
Nodes (11): GeminiProvider, m_callCounter, m_calls, m_finishReason, m_raw, m_text, public, Q_OBJECT (+3 more)

### Community 49 - "resolveArt"
Cohesion: 0.24
Nodes (11): QStringList, QVariant, QVariantMap, demarshall(), applyMetadata, applyPlayerProperties, applyRootProperties, onPropertiesChanged (+3 more)

### Community 50 - "notificationmodel.cpp"
Cohesion: 0.22
Nodes (15): Config, QByteArray, QHash, QObject, QString, quint32, quint64, close (+7 more)

### Community 51 - "shareserver.h"
Cohesion: 0.67
Nodes (4): qintptr, QTcpSocket, dropConnection, incomingConnection

### Community 52 - "ShareFile"
Cohesion: 0.18
Nodes (12): QList, QStringList, collect, offer, offerPaths, qint64, ShareFile, id (+4 more)

### Community 53 - "QTimer"
Cohesion: 0.67
Nodes (3): QNetworkAccessManager, QObject, GeminiProvider::GeminiProvider()

### Community 57 - "AiRequest"
Cohesion: 0.22
Nodes (9): AiRequest, baseUrl, effort, history, maxTokens, model, systemPrompt, tools (+1 more)

### Community 58 - "install.sh"
Cohesion: 0.52
Nodes (6): ask(), die(), note(), say(), install.sh script, warn()

### Community 60 - "sharecredentials.cpp"
Cohesion: 0.67
Nodes (5): QString, ShareCredentials, mint(), read(), ShareCredentials::load()

### Community 61 - "atollctl"
Cohesion: 0.83
Nodes (3): atollctl script, call(), usage()

### Community 62 - "Packaging"
Cohesion: 0.50
Nodes (3): Packaging, Publishing to the AUR, The assistant's client

## Knowledge Gaps
- **486 isolated node(s):** `role`, `text`, `toolCalls`, `toolResults`, `image` (+481 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **3 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `AiService` connect `AiService` to `ShellWindow`, `PermissionBroker`, `LockMonitor`, `Application`, `AiTurn`, `osdmonitor.cpp`, `aiservice.cpp`, `application.cpp`, `setState`?**
  _High betweenness centrality (0.206) - this node is a cross-community bridge._
- **Why does `ShareService` connect `ShareService` to `SharePeer`, `LockMonitor`, `ShareIdentity`, `Application`, `permissionbroker.cpp`, `ShareFile`, `shareservice.cpp`, `application.cpp`?**
  _High betweenness centrality (0.138) - this node is a cross-community bridge._
- **Why does `QML_UNCREATABLE` connect `LockMonitor` to `ShellWindow`, `LyricsService`, `AiService`, `Clock`, `MprisPlayer`, `IpcService`, `ShareService`, `Visualizer`, `Config`, `MprisManager`, `Battery`, `OsdMonitor`?**
  _High betweenness centrality (0.115) - this node is a cross-community bridge._
- **What connects `role`, `text`, `toolCalls` to the rest of the system?**
  _486 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `ShellWindow` be split into smaller, more focused modules?**
  _Cohesion score 0.06820119352088662 - nodes in this community are weakly interconnected._
- **Should `LyricsService` be split into smaller, more focused modules?**
  _Cohesion score 0.05672926447574335 - nodes in this community are weakly interconnected._
- **Should `AiService` be split into smaller, more focused modules?**
  _Cohesion score 0.02857142857142857 - nodes in this community are weakly interconnected._