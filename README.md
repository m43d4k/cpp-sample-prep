# cpp-audio-converter

GUI audio converter for common audio inputs with WAV / AIFF output.

## 概要
`cpp-audio-converter` は、macOS / Linux 向けの GUI オーディオコンバーターです。  
WAV / AIFF / FLAC / MP3 / OGG / CAF の読み込みと、WAV / AIFF の書き出し、サンプリングレート変換、bit depth 変換を行います。

変換エンジンには `r8brain-free-src` を使用し、音声ファイルの読み書きには `libsndfile` を使用します。  
GUI は `Slint`、ビルドは `CMake + Ninja` を前提としています。

## 対応OS
- macOS
- Linux

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
- 上書き保存
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

### Linux
- C++20 対応コンパイラ
- cmake
- ninja
- libsndfile
- pkg-config
- git

## ビルド
### macOS
    cmake --preset local-llvm
    cmake --build --preset local-llvm

### Linux
    cmake -S . -B build -G Ninja
    cmake --build build

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

## ライセンス
ライセンスは未定。
依存ライブラリのライセンス条件は各プロジェクトに従います。
