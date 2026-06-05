# Google Calendar Accessory

Qt/C++で作った、Googleカレンダー連携つきの小さなデスクトップアクセサリーです。月間カレンダー、予定の追加・編集・削除、ローカルのカウントダウンタイマーを備えています。

## 主な機能

- 半透明の月間カレンダー表示
- Google OAuth 2.0 + PKCEログイン
- Googleカレンダー予定の表示、追加、編集、削除
- 日付クリックでその日の予定をポップアップ表示
- 日付セルのツールチップで予定一覧を確認
- 5分ごとの自動更新
- 日付変更時の今日表示・表示月更新
- 任意分数のローカルカウントダウンタイマー
- タイマー終了時の通知音と点滅表示
- ウィンドウ位置、サイズ、透明度、タイマー分数の保存
- Windowsスタートアップ登録
- アプリアイコンつき

## フォルダー構成

```text
.
├─ assets/                         アプリアイコンなどの資産
├─ src/                            C++ソース
├─ app.rc                          Windowsリソース
├─ calendar_accessory.example.ini  設定ファイル例
├─ CMakeLists.txt                  CMakeプロジェクト定義
├─ LICENSE
└─ README.md
```

`build/` や `build-msvc/` はローカルのビルド成果物なのでGit管理対象外です。

## Google Cloud側の準備

1. Google Cloud Consoleでプロジェクトを作成します。
2. `Google Calendar API`を有効化します。
3. OAuth同意画面を設定します。
4. OAuthクライアントIDを作成します。
   - アプリケーションの種類: `デスクトップアプリ`
5. 生成されたクライアントIDを控えます。

## 初期設定

初回起動時に設定画面が開きます。右クリックメニューの `設定` からも開けます。

設定画面は3タブ構成です。

- `Google`: クライアントID、クライアントシークレット、カレンダーID、取得日数
- `表示`: 透明度、常に手前に表示、ウィンドウ位置リセット
- `起動`: Windows起動時に開始

`calendar_id` は通常 `primary` で動きます。別カレンダーを表示したい場合は、そのカレンダーIDを指定します。
`client_secret` は通常空で試せますが、Google側から要求された場合はCloud Consoleの値を入れてください。

予定の追加・編集・削除には書き込み権限が必要です。古い読み取り専用ログインのまま保存できない場合は、表示される確認から再ログインしてください。入力中の予定は再ログイン後に保存再試行されます。

## 操作

- `<` / `>` ボタンで前月・次月へ移動
- `今日` ボタンで今月へ戻る
- 月表示部分や左右ボタンを左ドラッグしてウィンドウ移動
- 日付をクリックして予定ポップアップを表示
- 予定ポップアップの `予定を追加` で新規予定を作成
- 既存予定をクリックして編集
- 既存予定の `削除` で削除
- 日付セルにマウスを乗せるとツールチップで予定を確認
- タイマー表示をクリックして開始・一時停止
- タイマー表示をダブルクリックして入力中の分数にリセット
- 分数入力を変更すると待機中・一時停止中のタイマー時間に即反映

## 設定ファイル

設定は実行ファイルと同じフォルダーの `calendar_accessory.ini` に保存されます。

主なセクション:

```ini
[Google]
client_id=YOUR_DESKTOP_CLIENT_ID.apps.googleusercontent.com
client_secret=
calendar_id=primary
days=7

[Display]
position=@Point(100 100)
size=@Size(430 430)
opacity=100
always_on_top=true

[Timer]
minutes=30
```

## ビルド例

```powershell
C:\Qt\Tools\CMake_64\bin\cmake.exe -S . -B build-msvc -DCMAKE_PREFIX_PATH=C:\Qt\6.11.1\msvc2022_64
C:\Qt\Tools\CMake_64\bin\cmake.exe --build build-msvc --config Release
C:\Qt\6.11.1\msvc2022_64\bin\windeployqt.exe build-msvc\Release\GoogleCalendarAccessory.exe
```

実行ファイル:

```text
build-msvc\Release\GoogleCalendarAccessory.exe
```

## ライセンス

このプロジェクトのソースコードはMIT Licenseです。詳細は [LICENSE](LICENSE) を参照してください。

このアプリはQtを使用しています。QtはLGPL/GPL/商用ライセンスで提供されています。Qtを改変せず動的リンクで利用する通常の配布であれば扱いやすいですが、バイナリ配布時はQtのライセンス表記や関連ライセンスファイルを同梱してください。
