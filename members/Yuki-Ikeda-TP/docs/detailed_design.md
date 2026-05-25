# 詳細設計書 — 組込み開発実習

<!-- 作成者: あなたの名前 / 日付: YYYY-MM-DD / グループ: 〇-〇 -->

> **このドキュメントの目的**
> 基本設計書（basic_design.md）で「**どのような構造で作るか**」を決めました。
> この詳細設計書では「**各処理を具体的にどう実装するか**」を決めます。
> 書き終わったとき、**コードの骨格がほぼ完成している**状態を目指してください。

> [!NOTE]
> **V字モデルにおける位置づけ**
> 詳細設計書 ←→ **単体テスト**（関数・部品ごとのテスト）が対応します。
> 「この関数が正しく動くか」の確認は Section 5 の単体テスト仕様書で計画します。
> ※ 必須機能全体が動くかの「結合テスト」は基本設計書（Section 6）に記載します。

---

## 0. 基本設計書との接続確認

| 項目 | basic_design.md から転記 |
|:--|:--|
| 作品タイトル | パスワード入力式LED判定装置 |
| 状態の種類（1-2 状態遷移から） | 4状態（待機中 / 入力中 / 判定中 / 結果表示） |
| 実装する関数の数（2-2 関数一覧から） | 9個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 23B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
  PIN_LED_GREEN : const int = 10  // 緑LED（正解時）
  PIN_LED_RED   : const int = 11  // 赤LED（不正解時）
  ROWS          : const byte = 4
  COLS          : const byte = 4
  rowPins[4]    : byte = {9, 8, 7, 6}  // キーパッド行
  colPins[4]    : byte = {5, 4, 3, 2}  // キーパッド列
  hexaKeys[4][4]: char = {{'1','2','3','A'}, {'4','5','6','B'}, {'7','8','9','C'}, {'*','0','#','D'}}

【状態定数】
  STATE_IDLE   : const int = 0  // 待機中
  STATE_INPUT  : const int = 1  // 入力中
  STATE_CHECK  : const int = 2  // 判定中
  STATE_RESULT : const int = 3  // 結果表示

【状態管理】
  currentState : int = STATE_IDLE

【入力管理】（basic_design.md 2-1 から転記）
  inputBuffer  : char[5] = ""      // 4桁 + 終端\0
  inputLength  : int = 0            // 0〜4
  password     : char[5] = "1234"  // 比較対象
  result       : bool = false

【タイマー（millis()用）】（basic_design.md 2-3 から転記）
  lastInputMillis  : unsigned long = 0  // 入力タイムアウト監視
  lastResultMillis : unsigned long = 0  // 結果表示開始時刻
  lastKeyMillis    : unsigned long = 0  // デバウンス判定

【閾値・周期定数】
  INPUT_TIMEOUT_MS : const unsigned long = 5000
  RESULT_HOLD_MS   : const unsigned long = 1000
  DEBOUNCE_DELAY   : const unsigned long = 50

【その他のフラグ・カウンター】
  lastKey      : char = '\0'
```

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### `setup()` — 初期化処理

```
【処理の流れ】
1. PIN_LED_GREEN, PIN_LED_RED を OUTPUT に設定し、初期値を LOW にする
2. キーパッドライブラリを hexaKeys / rowPins / colPins で初期化する
3. currentState を STATE_IDLE、inputBuffer を空文字、inputLength を 0 に初期化する
4. Serial.begin(9600) を実行する（デバッグ用）
5. 起動確認として緑LEDを200ms点灯して消灯する（任意）
```

---

### `loop()` — メインループ

> ※ loop() は「状態ごとに何をするか」だけ書く。細かい処理は各関数に任せる。

```
【処理の流れ】

＜毎ループ実行すること＞
  - now = millis() を取得
  - key = readKeypad() を取得
  - autoLedOff(now) を呼び、結果表示時間を監視

＜currentState が STATE_IDLE（待機中）のとき＞
  - LEDを両方消灯状態で維持
  - 数字キー入力があれば limitInputLength(key) を呼び、STATE_INPUT に遷移
  - 遷移時に lastInputMillis = now を更新

＜currentState が STATE_INPUT（入力中）のとき＞
  - 数字キーなら limitInputLength(key) を呼ぶ
  - '*' キーなら resetInputBuffer() を呼び、STATE_IDLE に戻る
  - '#' キーかつ inputLength == 4 なら STATE_CHECK に遷移
  - now - lastInputMillis >= INPUT_TIMEOUT_MS なら入力を破棄して STATE_IDLE に戻る
  - 数字入力を受け付けた場合は lastInputMillis = now を更新

＜currentState が STATE_CHECK（判定中）のとき＞
  - result = checkPassword(inputBuffer, password)
  - showResult(result) を呼ぶ
  - lastResultMillis = now を更新
  - STATE_RESULT に遷移

＜currentState が STATE_RESULT（結果表示）のとき＞
  - autoLedOff(now) により 1秒後の消灯と待機復帰を行う
```

---

### `readKeypad()` — キーパッド入力を1文字取得する

**basic_design.md 2-2 との対応：** F01 キーパッドから数字を入力できる

**引数：** なし

**戻り値：** char（入力なしは '\0'）

```
【処理の流れ】
1. キーパッドライブラリからキー値を取得する
2. 入力なしなら '\0' を返す
3. 直近入力時刻との差分を計算し、DEBOUNCE_DELAY 未満かつ同一キーなら無視する
4. 有効入力なら lastKeyMillis と lastKey を更新してキー値を返す

【エラー・異常ケース】
- 数字, '*', '#', A〜D 以外の値: '\0' を返して無視
```

---

### `limitInputLength(char key)` — 入力桁数を4桁までに制限する

**basic_design.md 2-2 との対応：** A01 入力中の桁数を制限できる

**引数：** key（char）: 入力キー

**戻り値：** void

```
【処理の流れ】
1. key が '0'〜'9' かを確認する
2. inputLength < 4 の場合のみ inputBuffer[inputLength] に追加
3. inputLength を +1 し、inputBuffer[inputLength] = '\0' を再設定
4. 4桁到達後は追加せず維持する

【エラー・異常ケース】
- 数字以外が来た場合: 何もしない
```

---

### `checkPassword(const char* input, const char* correct)` — パスワード一致判定を行う

**basic_design.md 2-2 との対応：** F02 パスワード一致判定

**引数：** input（const char*）: 入力文字列, correct（const char*）: 正解文字列

**戻り値：** bool

```
【処理の流れ】
1. input または correct が null なら false を返す
2. inputLength が 4 でない場合は false を返す
3. 4文字を順番に比較する
4. 全一致で true、1文字でも不一致なら false を返す

【エラー・異常ケース】
- 終端文字欠落の可能性: limitInputLength() 側で毎回 '\0' を入れて防止
```

---

### `showResult(bool isCorrect)` — 判定結果に応じてLEDを表示する

**basic_design.md 2-2 との対応：** F03 正解時に緑LED点灯 / F04 不正解時に赤LED点灯

**引数：** isCorrect（bool）: 判定結果

**戻り値：** void

```
【処理の流れ】
1. 緑LEDと赤LEDを一度LOWにする
2. isCorrect が true なら緑LEDをHIGHにする
3. isCorrect が false なら赤LEDをHIGHにする
4. 結果表示開始時刻 lastResultMillis を更新する

【エラー・異常ケース】
- 点灯しない場合: ピン定義と抵抗配線を確認し、Serialに状態を出力
```

---

### `autoLedOff(unsigned long now)` — 一定時間後にLEDを自動消灯する

**basic_design.md 2-2 との対応：** A02 一定時間後にLEDを自動消灯できる

**引数：** now（unsigned long）: 現在時刻

**戻り値：** void

```
【処理の流れ】
1. currentState が STATE_RESULT のときのみ処理する
2. now - lastResultMillis >= RESULT_HOLD_MS を判定する
3. 条件成立なら両LEDをLOWにし、resetInputBuffer() を呼ぶ
4. currentState を STATE_IDLE に戻す

【エラー・異常ケース】
- millis() オーバーフロー時も差分判定で動作継続可能
```

---

### `resetInputBuffer()` — 入力バッファを初期化する

**basic_design.md 2-2 との対応：** loop内の再入力・リセット処理を共通化

**引数：** なし

**戻り値：** void

```
【処理の流れ】
1. inputBuffer の全要素を '\0' にする
2. inputLength を 0 に戻す
3. lastKey を '\0' に戻す

【エラー・異常ケース】
- なし（固定長配列初期化のみ）
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  キーパッドも機械接点のため、押下直後に同一キーが短時間で複数回検出されることがある。
  50ms以内の同一キー再入力は同じ1回として扱い、重複入力を防ぐ。

【処理の流れ】
  1. key = readKeypad() で入力キー取得
  2. key が '\0' なら未入力として終了
  3. now - lastKeyMillis < DEBOUNCE_DELAY かつ key == lastKey なら無視
  4. 条件を満たさない場合のみ有効入力として採用
  5. lastKeyMillis = now, lastKey = key を更新

【必要な変数（Section 1 に追加済みか確認）】
  lastKeyMillis : unsigned long
  lastKey       : char
  DEBOUNCE_DELAY: const unsigned long = 50
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  delay() を使わず、入力監視・判定・表示を並行して処理するため、
  すべて「現在時刻 - 前回時刻 >= 閾値」の差分判定で管理する。

【処理の流れ（本システム）】
  1. now = millis() を毎ループ取得
  2. 入力中は now - lastInputMillis >= INPUT_TIMEOUT_MS でタイムアウト判定
  3. 結果表示中は now - lastResultMillis >= RESULT_HOLD_MS で消灯判定
  4. 条件未達なら状態維持して次ループで再判定

【自分のシステムで millis() を使う処理】
  - 入力タイムアウト（5秒）
  - 判定後LED自動消灯（1秒）
  - キー入力デバウンス（50ms）
```

---

### 3-3. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【処理の流れ】
1. 数字キー入力時のみバッファへ追加する
2. '*' キー入力で入力全消去し、待機状態へ戻す
3. '#' キー入力で4桁入力済みの場合のみ判定に進む

【入力値と出力値の関係】
  数字: バッファ更新（LED変化なし）
  '*': 入力キャンセル
  '#': 判定開始トリガ
```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | キー入力が取得できているか | `readKeypad()` | `Serial.println(key);` |
| 2 | 入力バッファが想定通りか | `limitInputLength()` | `Serial.println(inputBuffer);` |
| 3 | 状態遷移が正しいか | `loop()` | `Serial.println(currentState);` |
| 4 | 判定結果が正しいか | `checkPassword()` | `Serial.println(result);` |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readKeypad() | キー「1」を1回押す | `'1'` が返る | | [ ] |
| 2 | readKeypad() | 同一キーを素早く連打する | デバウンスにより過剰入力されない | | [ ] |
| 3 | limitInputLength() | 数字を5回入力する | 4桁までしか格納されない | | [ ] |
| 4 | resetInputBuffer() | `*` キーを押す | inputBuffer が空文字、inputLength=0 | | [ ] |
| 5 | checkPassword() | `1234` を入力して判定 | true が返る | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | showResult(true) | 正解判定後に呼ぶ | 緑LEDが点灯、赤LEDは消灯 | | [ ] |
| 2 | showResult(false) | 不正解判定後に呼ぶ | 赤LEDが点灯、緑LEDは消灯 | | [ ] |
| 3 | autoLedOff() | 結果表示後に1秒待つ | 両LEDが自動消灯し待機状態へ戻る | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | delay()による処理停止がないか | 結果表示中に次キー入力を試す | loopが継続し、次状態へ遷移できる | | [ ] |
| 2 | 入力タイムアウト精度 | 入力開始後5秒放置する | 入力が破棄され待機状態に戻る | | [ ] |
| 3 | 結果表示時間の精度 | 判定後LED点灯時間を計測する | 約1秒後に自動消灯する | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**
- `#` 押下時は4桁入力済みかのチェックを必須にする
- `char[5]` は終端 `\0` を毎回維持しないと誤判定の原因になる
- `STATE_CHECK` は即時処理して停滞しない状態遷移にする
- 時間判定はすべて差分比較に統一すると `millis()` オーバーフローにも強い

**対応した内容：**
- `inputLength == 4` のときだけ判定に進む設計にした
- `limitInputLength()` で終端文字を毎回再設定する仕様にした
- 判定直後に `STATE_RESULT` へ必ず遷移するフローにした
- タイムアウトと自動消灯を `now - lastXxx >= 閾値` で統一した

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**
- 正常系だけでなく、5桁目入力・4桁未満で `#` 押下など境界値テストが必要
- `*` によるキャンセル後の状態復帰確認も必要
- タイミング系は許容誤差を決めて測定するのが望ましい

**対応した内容：**
- 入力上限テスト（5桁目無視）を追加した
- `*` キーによるバッファ初期化テストを追加した
- タイムアウト精度と結果表示時間テストを追加した

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 | 状態遷移図に「異常系」や「リセット経路」が明記されていない |  | 状態遷移表に「*」キーやタイムアウトによるリセット遷移を追記した |
| 2 | 変数名が抽象的で分かりづらい箇所がある（例：result, inputBuffer） |  | コメントで用途を明記し、変数宣言部に説明を追加した |
| 3 | テスト仕様に「4桁未満で#を押した場合」や「誤ったパスワード入力時」の観点が抜けている |  | 単体テスト仕様5-1, 5-2に該当ケースを追加した |
| 4 | LED消灯タイミングの仕様が曖昧 |  | autoLedOff()の設計に消灯条件を明記し、テスト項目も追加した |

### 7-2. レビューを受けて変更した点
  
- 状態遷移図・表に「*」キーやタイムアウトによるリセット経路を明記
- 変数宣言部に用途コメントを追加
- テスト仕様に「4桁未満で#」「誤パスワード」など境界値・異常系を追加
- autoLedOff()の消灯条件を明記し、テスト観点も補強

---

*初版: YYYY-MM-DD / AIレビュー: YYYY-MM-DD / グループレビュー後更新: YYYY-MM-DD*
