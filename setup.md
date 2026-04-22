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

補足:
- Linux 対応はひとまず見送る

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

ローカル起動:
    ./build/cpp_audio_converter

補足:
- `./build/cpp_audio_converter` は `build/SamplePrep.app/Contents/MacOS/SamplePrep` を起動するランチャー
- `.app` の build 自体は `build/SamplePrep.app`

### 6. `.app` の stage
配布前の動作確認に使える、依存ライブラリを同梱した `.app` を生成する。

    cmake --build --preset local-llvm --target stage_macos_app

出力先:
- `build/dist/stage/SamplePrep.app`

### 7. 配布用フォルダ一式を生成
`build/dist/release/` に `SamplePrep.app`、`README.txt`、配布用 ZIP をまとめて出力する。

    cmake --build --preset local-llvm --target package_macos_release

出力先:
- `build/dist/release/SamplePrep.app`
- `build/dist/release/README.txt`
- `build/dist/release/SamplePrep-0.1.0-macOS-arm64.zip`

補足:
- ZIP は無圧縮で生成する
- `build/dist/release/` に配布用ファイル一式をまとめて出力する
- `build/dist/release/SamplePrep-0.1.0-macOS-arm64.zip` の中には `.app` と `README.txt` を含める

補足:
- `stage_macos_app` は `fixup_bundle` を使って `libsndfile` と従属 dylib をアプリ内へコピーする
- `package_macos_release` は `stage_macos_app` を前提にしてリリース用ファイルをまとめる
- bundle identifier を変える場合は configure 時に `-DSAMPLEPREP_BUNDLE_IDENTIFIER=...` を付ける

### 8. 未署名配布時の前提
現時点の標準フローは未署名の `.app` / ZIP を配布する形とする。

- Apple Silicon 向けの配布物は `package_macos_release` で生成する
- 初回起動時に macOS にブロックされた場合は `System Settings` → `Privacy & Security` → `Open Anyway` を案内する
- これは Apple の公式案内に沿った運用とする

Apple の案内:
- https://support.apple.com/en-us/HT202491

### 10. Optional: codesign / notarization
Developer ID 配布へ切り替える場合は以下を別途用意する。

- `Developer ID Application` 証明書
- `xcrun notarytool` 用の keychain profile

その場合のみ以下を使う。

    CODESIGN_IDENTITY="Developer ID Application: Example" \
    NOTARYTOOL_PROFILE="notary-profile" \
    ./scripts/macos/sign_and_notarize.sh build/dist/stage/SamplePrep.app

`NOTARYTOOL_PROFILE` を省略すると、codesign と検証だけを行う。

---

## Slint の扱い
このプロジェクトでは、Slint は CMake の `FetchContent` で取り込む前提とする。

補足:
- Slint をソースからビルドするため、`cargo` / `rustc` が必要
- macOS では `brew install rust` を使う

方針:
- まずは外部に事前インストールしない
- `CMakeLists.txt` から取得してビルドする
- `.slint` ファイルは `slint_target_sources(...)` で対象ターゲットに追加する
- 実行ターゲットは `Slint::Slint` にリンクする

最小構成のイメージ:

    cmake_minimum_required(VERSION 3.21)
    project(SamplePrep LANGUAGES CXX)

    include(FetchContent)
    FetchContent_Declare(
        Slint
        GIT_REPOSITORY https://github.com/slint-ui/slint.git
        GIT_TAG release/1
        SOURCE_SUBDIR api/cpp
    )
    FetchContent_MakeAvailable(Slint)

    add_executable(sampleprep
        src/main.cpp
    )

    slint_target_sources(sampleprep
        ui/app-window.slint
    )

    target_link_libraries(sampleprep PRIVATE Slint::Slint)

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

### 2. build
    cmake --build --preset local-llvm

### 3. 確認項目
- CMake configure が通る
- Slint が取得・生成される
- `libsndfile` が解決される
- `.app` が生成される
- `stage_macos_app` が通る
- `package_macos_release` が通る

---

## 現時点の方針
- C++20 を主言語とする
- GUI は Slint
- 音声ファイル I/O は libsndfile
- サンプルレート変換は r8brain-free-src
- ビルドは CMake + Ninja
- macOS は Homebrew 版 LLVM をローカルプリセットで使用してよい
- Python / Node.js は必須にしない
- Linux 対応は後続タスクとして扱う
