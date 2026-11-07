# RedBunny Training Dataset Card

Generated from the RedBunny repository by `TRAIN_IA_MODEL/build.py`.

Hugging Face dataset: https://huggingface.co/datasets/mohamedboukerche55/redbunny-training-data

## Counts

| Split | Examples |
|---|---:|
| Total | 167 |
| Train | 148 |
| Eval | 19 |

## Categories

| Category | Examples |
|---|---:|
| `arrays` | 8 |
| `basics` | 1 |
| `classes` | 2 |
| `cli` | 1 |
| `code-output` | 13 |
| `compile-error` | 15 |
| `control-flow` | 5 |
| `core` | 2 |
| `errors` | 8 |
| `example` | 12 |
| `functions` | 4 |
| `includes` | 3 |
| `limitations` | 1 |
| `manual` | 16 |
| `operators` | 2 |
| `output` | 3 |
| `programs` | 14 |
| `stdlib` | 37 |
| `strings` | 1 |
| `syntax` | 2 |
| `test-output` | 13 |
| `types` | 4 |

## Formats

- `chat.jsonl` - OpenAI-style `messages` records with a RedBunny system prompt.
- `train.jsonl` - deterministic 90% training split.
- `eval.jsonl` - deterministic 10% evaluation split.
- `alpaca.jsonl` - `instruction`, `input`, `output` records.
- `concise.jsonl` - short syntax-focused chat records.
- `moh_gpt.jsonl` - concise messages-only records compatible with `moh-gpt/train.sh`.
- `plain.txt` - plain text corpus for continued pretraining.

## Regenerate

```bash
python3 TRAIN_IA_MODEL/build.py
```

The generator scans `docs/`, `examples/`, `lib/`, `tests/`, `README.md` and the
curated `TRAIN_IA_MODEL/seed.jsonl`. Add a file or a seed record, rerun the
build, and the dataset is updated.
