# SamplePrep

GUI audio converter for common audio inputs with WAV / AIFF output.

## 概要
`SamplePrep` は、macOS 向けの GUI オーディオコンバーターです。  
WAV / AIFF / FLAC / MP3 / OGG / CAF の読み込みと、WAV / AIFF の書き出し、サンプリングレート変換、bit depth 変換を行います。

Linux 対応はひとまず見送っています。

変換エンジンには `r8brain-free-src` を使用し、音声ファイルの読み書きには `libsndfile` を使用します。  
GUI は `Slint`、ビルドは `CMake + Ninja` を前提としています。

## 対応OS
- macOS

## 主な機能
- GUI から音声ファイルを一括変換
- 入力
  - 複数ファイル
  - 単一フォルダ
- 出力フォーマット
  - WAV
  - AIFF
- サンプリングレート変換
  - 44100
  - 48000
  - 88200
  - 96000
- bit depth 変換
  - 8 bit PCM
  - 16 bit PCM
  - 24 bit PCM
  - 32 bit PCM
- 使用 CPU コア数の選択
  - All
  - 1 / 2 / 3 / 4 / 6 / 8 / 10
- 上書き保存
- 元ファイルと同じディレクトリへの別名保存
  - prefix / postfix を付けて保存
- 別名保存
  - prefix / postfix のどちらか一方を指定
  - フォルダ入力時は出力先でも相対フォルダ構造を維持
- 同一条件ファイルのスキップ
- ファイルごとのログ表示
- macOS での入力ファイル / 入力フォルダのドラッグアンドドロップ

## v1 の仕様
- 変換対象はモノラル / ステレオファイル
- 多チャンネルは対象外
- 入力形式は WAV / AIFF / FLAC / MP3 / OGG / CAF
- フォルダ入力は配下を再帰的に対象とする
- 非対応形式、多チャンネル入力、破損ファイルはスキップしてログに記録
- 上書き保存は一時ファイル経由で安全に行う
- バックアップファイルは作成しない
- 同一条件のファイルはスキップしてログに記録する
- フォーマットのみ異なる場合は変換対象とし、音声条件を維持したままコンテナのみ変更する
- ファイル単位の並列変換に対応し、各ジョブは独立した resampler / input handle / output handle を持つ
- `Use CPU cores = All` の場合、worker 数は `min(std::thread::hardware_concurrency(), 入力ファイル数)` とする
- 入力ファイル群、入力フォルダ、出力フォルダはネイティブダイアログから選択できる
- macOS では入力ファイル群、入力フォルダをドラッグアンドドロップでも指定できる
- 上書き時にコンテナ変更で拡張子が変わる場合は、同一ディレクトリ内で拡張子を出力形式に合わせる

## 技術構成
- Language: C++20
- GUI: Slint
- Audio File I/O: libsndfile
- Sample Rate Conversion: r8brain-free-src
- Build: CMake + Ninja

Sample rate converter designed by Aleksey Vaneev of Voxengo.

## ディレクトリ構成
    .
    ├── CMakeLists.txt
    ├── CMakeUserPresets.json
    ├── third_party/
    │   └── r8brain-free-src/
    ├── src/
    │   ├── main.cpp
    │   ├── core/
    │   ├── audio/
    │   └── util/
    ├── ui/
    │   └── app-window.slint
    ├── docs/
    └── build/

## セットアップ
詳細は `setup.md` を参照してください。

### macOS
- Command Line Tools
- Homebrew
- llvm
- cmake
- ninja
- libsndfile

## ビルド
### macOS
    cmake --preset local-llvm
    cmake --build --preset local-llvm

ビルド後、ローカル起動用の `.app` は `build/SamplePrep.app` に生成される。
ターミナルからは `./build/cpp_audio_converter` で起動できる。

### self-contained `.app` の stage
    cmake --build --preset local-llvm --target stage_macos_app

出力先:
- `build/dist/stage/SamplePrep.app`

### 配布用 ZIP の生成
    cmake --build --preset local-llvm --target package_macos_zip

出力先:
- `build/dist/SamplePrep-0.1.0-macOS-arm64.zip`

### release ディレクトリの生成
    cmake --build --preset local-llvm --target package_macos_release

出力先:
- `build/dist/release/SamplePrep.app`
- `build/dist/release/README.txt`
- `build/dist/release/SamplePrep-0.1.0-macOS-arm64.zip`

補足:
- stage / package 時に `libsndfile` とその依存ライブラリを `.app` 内へ同梱する
- bundle identifier の既定値は `com.example.sampleprep`
- 必要なら configure 時に `-DSAMPLEPREP_BUNDLE_IDENTIFIER=com.your-domain.sampleprep` を指定して上書きできる

## 配布方針
現時点の標準配布方針は以下とする。

- Apple Silicon 向け
- `.app` または ZIP を未署名のまま配布する
- 初回起動時に macOS がブロックした場合は `System Settings` → `Privacy & Security` → `Open Anyway` を案内する

### 初回起動の案内
未署名のため、ダウンロードした環境では初回起動時に macOS の警告が出る場合がある。

その場合は以下の手順で起動する。

1. アプリを一度開いてブロックされることを確認する
2. `System Settings` を開く
3. `Privacy & Security` を開く
4. 下部に表示される `Open Anyway` を押す
5. 確認ダイアログで再度 `Open` を押す

Apple の案内:
- https://support.apple.com/en-us/HT202491

### Optional: codesign / notarization
Developer ID 配布を行う場合は補助スクリプトを使える。

    CODESIGN_IDENTITY="Developer ID Application: Example" \
    NOTARYTOOL_PROFILE="notary-profile" \
    ./scripts/macos/sign_and_notarize.sh build/dist/stage/SamplePrep.app

これは現時点の標準手順ではない。

## 文書
- `spec.md`
  - プロダクト仕様
- `setup.md`
  - 開発環境構築
- `architecture.md`
  - 技術方針、依存関係、責務分割

## 開発方針
- 主言語は C++
- GUI は C++ + Slint
- `r8brain-free-src` は必ず内蔵する
- `libsndfile` は外部依存として扱う
- Python / Node.js は必須としない
- Linux 対応は後続タスクとして扱う

## ライセンス
ライセンスは未定。
依存ライブラリのライセンス条件は各プロジェクトに従います。
