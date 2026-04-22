# 開発環境方針

## 採用構成
- 言語: C++20
- GUI: Slint
- 音声ファイルI/O: libsndfile
- サンプルレート変換: r8brain-free-src
- ビルド: CMake + Ninja

## 対応OS
- macOS
- Linux は将来対応予定とし、現時点では対象外

## 役割分担
- Slint
  - 設定画面
  - 出力先選択
  - CPU worker 数の選択
  - ログ表示
  - 進捗表示
- libsndfile
  - WAV / AIFF / FLAC / MP3 / OGG / CAF の読み込み
  - WAV / AIFF の書き出し
  - PCM bit depth の処理
- r8brain-free-src
  - サンプルレート変換
- CMake
  - プロジェクト構成管理
  - 依存関係の接続
  - macOS ビルド構成の管理

## 開発時の基本ツール
- コンパイラ
  - macOS: clang++
- ビルドツール
  - CMake
  - Ninja

## この構成の意味
- r8brain-free-src を最短距離で内蔵できる
- C++ 単体で完結しやすい
- GUI は wxWidgets より拡張しやすい
- 将来的な設定追加や画面拡張に対応しやすい

## 実装単位の分離方針
- ui/
  - Slint UI
  - 画面イベント
- core/
  - 設定検証
  - 変換ジョブ管理
  - ファイル単位の並列実行制御
  - ログ管理
- audio/
  - libsndfile と r8brain の橋渡し
- util/
  - パス処理
  - ファイル名ルール
  - 一時ファイル処理
  - ネイティブファイル / フォルダ選択の薄い OS ラッパ
  - ネイティブドラッグアンドドロップの薄い OS ラッパ

## 依存関係の考え方
- r8brain-free-src はプロジェクト内に同梱
- libsndfile は外部依存として扱う
- Slint は CMake から組み込む

## 技術方針として確定した内容
- UI は C++ + Slint
- 音声変換コアは C++
- r8brain-free-src を必ず内蔵
- 当面は macOS を正式対象として進める

## 並列実行方針
- UI スレッドと変換実行スレッドは分離する
- 変換の並列化はファイル単位で行う
- 1 ファイル内のチャンネル分割並列化は行わない
- 各ジョブは独立した input handle / output handle / resampler を持つ
- `Use CPU cores = All` の場合は `std::thread::hardware_concurrency()` を使用し、実 worker 数は入力ファイル数を上限とする
