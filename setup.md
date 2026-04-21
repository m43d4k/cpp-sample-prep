# setup.md

## 目的
このプロジェクトのローカル開発環境をセットアップする。

対象プロジェクト:
- C++20
- Slint
- libsndfile
- r8brain-free-src
- CMake + Ninja

対象OS:
- macOS
- Linux

---

## このプロジェクトで必須のもの
- C++20 対応コンパイラ
- CMake
- Ninja
- Rust / Cargo
- libsndfile
- Git

このプロジェクトでは、Python / Node.js は必須としない。

Slint は CMake から組み込み、r8brain-free-src はリポジトリ内に同梱する前提とする。

---

## macOS

### 1. Command Line Tools を入れる
    xcode-select --install

### 2. 必要ツールを入れる
Homebrew を使う前提。

    brew install llvm cmake ninja rust libsndfile

### 3. 確認
    /opt/homebrew/opt/llvm/bin/clang++ --version
    cmake --version
    ninja --version
    cargo --version
    pkg-config --modversion sndfile

### 4. ローカル用 CMakeUserPresets.json を置く
リポジトリ直下に `CMakeUserPresets.json` を作成する。

    {
      "version": 3,
      "configurePresets": [
        {
          "name": "local-llvm",
          "generator": "Ninja",
          "binaryDir": "${sourceDir}/build",
          "environment": {
            "CC": "/opt/homebrew/opt/llvm/bin/clang",
            "CXX": "/opt/homebrew/opt/llvm/bin/clang++"
          }
        }
      ],
      "buildPresets": [
        {
          "name": "local-llvm",
          "configurePreset": "local-llvm"
        }
      ]
    }

注意:
- このファイルはローカル環境用なので、基本的に Git 管理しない
- Homebrew の配置が異なる場合は `CC` / `CXX` のパスを自分の環境に合わせて修正する

### 5. configure / build
    cmake --preset local-llvm
    cmake --build --preset local-llvm

---

## Linux

### 1. 必要ツールを入れる
ディストリビューションごとの方法で以下を導入する。

- C++20 対応コンパイラ
  - `g++` または `clang++`
- `cmake`
- `ninja`
- `rustc` / `cargo`
- `libsndfile` の開発パッケージ
- `pkg-config`
- `git`

ネイティブのファイル / フォルダ選択ダイアログを使う場合は、以下のいずれかがあるとよい。

- `zenity`
- `kdialog`

### 2. 確認
gcc 系を使う場合:

    g++ --version

clang 系を使う場合:

    clang++ --version

共通:

    cmake --version
    ninja --version
    cargo --version
    pkg-config --modversion sndfile

### 3. configure / build
gcc を使う例:

    cmake -S . -B build -G Ninja
    cmake --build build

clang を使う例:

    CC=clang CXX=clang++ cmake -S . -B build -G Ninja
    cmake --build build

必要であれば Linux でも `CMakeUserPresets.json` を使ってローカルプリセット化してよい。

---

## Slint の扱い
このプロジェクトでは、Slint は CMake の `FetchContent` で取り込む前提とする。

補足:
- Slint をソースからビルドするため、`cargo` / `rustc` が必要
- macOS では `brew install rust`、Linux ではディストリビューション標準の Rust パッケージまたは `rustup` を使う

方針:
- まずは外部に事前インストールしない
- `CMakeLists.txt` から取得してビルドする
- `.slint` ファイルは `slint_target_sources(...)` で対象ターゲットに追加する
- 実行ターゲットは `Slint::Slint` にリンクする

最小構成のイメージ:

    cmake_minimum_required(VERSION 3.21)
    project(audio_converter LANGUAGES CXX)

    include(FetchContent)
    FetchContent_Declare(
        Slint
        GIT_REPOSITORY https://github.com/slint-ui/slint.git
        GIT_TAG release/1
        SOURCE_SUBDIR api/cpp
    )
    FetchContent_MakeAvailable(Slint)

    add_executable(audio_converter
        src/main.cpp
    )

    slint_target_sources(audio_converter
        ui/app-window.slint
    )

    target_link_libraries(audio_converter PRIVATE Slint::Slint)

---

## libsndfile の扱い
libsndfile は外部依存として扱う。

CMake では、環境に応じて以下のいずれかで解決する。

### 方針A
`pkg-config` を使う

### 方針B
`find_package` または手動リンクで解決する

まずは `pkg-config` ベースで進めるのが簡単。

---

## r8brain-free-src の扱い
r8brain-free-src はリポジトリ内に同梱する。

想定配置:

    third_party/r8brain-free-src/

方針:
- システム全体にはインストールしない
- プロジェクト内依存として扱う
- サンプルレート変換専用に使う
- ドキュメント上では以下のクレジットを残す
  - `Sample rate converter designed by Aleksey Vaneev of Voxengo`

---

## 推奨ディレクトリ構成
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
    ├── build/
    └── docs/

補足:
- `CMakeUserPresets.json` はローカル専用
- `build/` は生成物なので Git 管理しない

---

## .gitignore に最低限入れるもの
    build/
    CMakeUserPresets.json
    .DS_Store

必要なら後で CMake / IDE / editor ごとの項目を追加する。

---

## 最初の動作確認
セットアップ後、最低限ここまで通ればよい。

### 1. configure
    cmake --preset local-llvm

または Linux:

    cmake -S . -B build -G Ninja

### 2. build
    cmake --build --preset local-llvm

または:

    cmake --build build

### 3. 確認項目
- CMake configure が通る
- Slint が取得・生成される
- `libsndfile` が解決される
- 実行ファイルが生成される

---

## 現時点の方針
- C++20 を主言語とする
- GUI は Slint
- 音声ファイル I/O は libsndfile
- サンプルレート変換は r8brain-free-src
- ビルドは CMake + Ninja
- macOS は Homebrew 版 LLVM をローカルプリセットで使用してよい
- Python / Node.js は必須にしない
