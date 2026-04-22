README

------------------------------------------------------------------
English
------------------------------------------------------------------

Sample file converter for macOS.

Reads WAV / AIFF / FLAC / MP3 / OGG / CAF and converts them to WAV / AIFF.
Batch-converts samples by unifying sample rate and bit depth, then exports them as WAV / AIFF.
Uses `r8brain-free-src` for sample rate conversion.

Usage
1. Launch `SamplePrep.app`.
2. Add files to convert with `Add files`, or choose a target folder with `Add directory`. Drag and drop is also supported.
3. Set the output destination, file naming rule, Sample rate, Output format, and Bit depth as needed.
4. Run `Convert`.

First launch
If macOS shows a warning that the developer cannot be verified or that Apple cannot check the app for malicious software, use the following steps.

1. Open `System Settings`.
2. Open `Privacy & Security`.
3. Select `Open Anyway`.
4. In the confirmation dialog, select `Open` again.

Apple support
https://support.apple.com/en-us/HT202491

Supported input formats
WAV, AIFF, FLAC, MP3, OGG, CAF

Output formats
WAV, AIFF

Notes
- v1 supports mono and stereo input.
- Multichannel input, unsupported formats, and corrupted files are skipped and recorded in the log.
- Files whose output format, sample rate, and bit depth already match the input are skipped and recorded in the log.


------------------------------------------------------------------
Japanese
------------------------------------------------------------------

macOS 用のサンプルファイル変換ツール。

WAV / AIFF / FLAC / MP3 / OGG / CAF を読み込み、WAV / AIFF に変換。
サンプルファイルのサンプリングレートや bit depth をそろえ、WAV / AIFF へまとめて書き出す。
サンプルレート変換には `r8brain-free-src` を使用する。

使い方
1. `SamplePrep.app` を起動する。
2. `Add files` で変換するファイルを追加するか、`Add directory` で対象フォルダを指定する。ドラッグアンドドロップにも対応。
3. 必要に応じて出力先、ファイル名ルール、Sample rate、Output format、Bit depth を設定する。
4. `Convert` を実行する。

初回起動について
初回起動時に開発元を検証できない、または悪質なソフトウェアかどうかを Apple で確認できないという警告が表示された場合は、以下の手順で起動する。

1. `システム設定` を開く。
2. `プライバシーとセキュリティ` を開く。
3. `このまま開く` を選ぶ。
4. 確認ダイアログで再度 `開く` を選ぶ。

Apple の案内
https://support.apple.com/en-us/HT202491

対応入力形式
WAV, AIFF, FLAC, MP3, OGG, CAF

出力形式
WAV, AIFF

注意
- v1 はモノラル / ステレオ入力に対応。
- 多チャンネル入力、非対応形式、破損ファイルはスキップしてログに記録する。
- 出力フォーマット、サンプルレート、bit depth が入力と同一のファイルはスキップしてログに記録する。
