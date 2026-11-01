# RedBunny AI Training Data

This directory turns the RedBunny repository into an instruction-tuning dataset
that a local model can learn from. It is dependency-free to generate and ships
with several common training formats.

**Hugging Face dataset:** https://huggingface.co/datasets/mohamedboukerche55/redbunny-training-data

## Quick start

From the repository root:

```bash
python3 TRAIN_IA_MODEL/build.py
```

That scans the manual, examples, standard library, tests and the curated
`seed.jsonl`, then writes:

| File | Format | Use |
|---|---|---|
| `data/chat.jsonl` | OpenAI-style `messages` | Chat fine-tuning |
| `data/train.jsonl` | `messages`, 90% split | Main training split |
| `data/eval.jsonl` | `messages`, 10% split | Validation / eval |
| `data/alpaca.jsonl` | `instruction`, `input`, `output` | Alpaca-style trainers |
| `data/concise.jsonl` | short `messages` records | Fast syntax-focused training |
| `data/moh_gpt.jsonl` | concise `messages` only | `moh-gpt/train.sh` pipeline |
| `data/plain.txt` | plain text | Continued pretraining / llama.cpp |
| `data/manifest.json` | JSON | Counts, hashes and source list |
| `data/dataset_card.md` | Markdown | Dataset summary |

The split is deterministic, so rebuilding does not reshuffle examples.

## Auto-update while editing

Run the watcher in a terminal:

```bash
python3 TRAIN_IA_MODEL/watch.py
```

It rebuilds automatically when files under `docs/`, `examples/`, `lib/`,
`tests/`, `README.md`, or `TRAIN_IA_MODEL/seed.jsonl` change.

The GitHub Actions workflow `.github/workflows/train-data.yml` also regenerates
and commits `TRAIN_IA_MODEL/data/` when source files are pushed to `main`.

To check whether the committed data is stale without writing:

```bash
python3 TRAIN_IA_MODEL/build.py --check
```

## Adding training examples

### Add a curated question and answer

Append one JSON object per line to `seed.jsonl`:

```json
{"category":"basics","instruction":"How do I print a value?","input":"","output":"Use `writeln` from `@include \"std\"`."}
```

`instruction`, `output`, and `category` are used directly. Keep `input` empty
unless the question needs extra context.

### Add source material

Just add files to the repository:

- New manual sections in `docs/DOCUMENTATION.md` are extracted automatically.
- New `.redb` files in `examples/` become complete-program examples.
- New test cases in `tests/cases/` become both "write this program" and
  "what does this program print" pairs.
- New compile-error tests in `tests/errors/` become debugging pairs.
- New standard-library functions in `lib/*.redb` become function-reference
  pairs.

Then run the build or leave the watcher running.

## Train a local model

Install the optional Hugging Face training dependencies:

```bash
python3 -m pip install -r TRAIN_IA_MODEL/requirements-train.txt
```

Run LoRA fine-tuning on a local causal language model:

```bash
python3 TRAIN_IA_MODEL/train_lora.py \
  --model /path/to/local/model \
  --data TRAIN_IA_MODEL/data \
  --output TRAIN_IA_MODEL/output/redbunny-lora
```

Useful options:

```bash
--max-length 2048
--epochs 3
--batch-size 1
--grad-accum 8
--learning-rate 2e-4
--lora-r 16
--lora-alpha 32
--target-modules q_proj,k_proj,v_proj,o_proj
--bf16
```

Adjust `--target-modules` for your architecture. For GPT-2 style models, use
`c_attn`; for many Llama/Mistral/Qwen models the default works.

For continued pretraining or tools that expect plain text, use
`data/plain.txt`. For chat-template trainers, use `data/train.jsonl` and
`data/eval.jsonl`.

### Use with the local `moh-gpt` trainer

`data/moh_gpt.jsonl` is the concise syntax-focused set and uses the same schema
as `moh-gpt/training/data.jsonl`:

```json
{"messages":[{"role":"user","content":"..."},{"role":"assistant","content":"..."}]}
```

Point its trainer at the file directly:

```bash
cd /path/to/moh-gpt
./train.sh --data /path/to/redbunny/TRAIN_IA_MODEL/data/moh_gpt.jsonl --max 153
```

For the LoRA path, use the same file:

```bash
./train.sh --method lora --data /path/to/redbunny/TRAIN_IA_MODEL/data/moh_gpt.jsonl
```

## Data conventions

- The system prompt is in `prompts/system.txt`.
- Code answers use fenced `redb` blocks.
- The generated data is intentionally committed so users can download and
  train without running the generator.
- Do not edit files under `data/` by hand; they are overwritten by the build.

## Current coverage

The generator covers:

- core syntax, types, operators and control flow
- arrays, nested arrays, resizing and aliasing
- functions, signatures, recursion and includes
- `mathlib` and `algorithm`
- complete programs, expected outputs and compiler errors

Run `python3 TRAIN_IA_MODEL/build.py` and read
`data/dataset_card.md` for the current counts.
