#!/usr/bin/env python3
"""Minimal LoRA supervised fine-tuning for the generated RedBunny chat data.

This script is optional. The dataset itself has no Python dependencies.

Install:
    python3 -m pip install -r TRAIN_IA_MODEL/requirements-train.txt

Example:
    python3 TRAIN_IA_MODEL/train_lora.py \
        --model /path/to/local/model \
        --data TRAIN_IA_MODEL/data \
        --output TRAIN_IA_MODEL/output/redbunny-lora
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", required=True, help="local model path or Hugging Face id")
    parser.add_argument("--data", default="TRAIN_IA_MODEL/data", help="data directory or train.jsonl")
    parser.add_argument("--output", default="TRAIN_IA_MODEL/output/redbunny-lora")
    parser.add_argument("--max-length", type=int, default=2048)
    parser.add_argument("--epochs", type=float, default=3.0)
    parser.add_argument("--batch-size", type=int, default=1)
    parser.add_argument("--grad-accum", type=int, default=8)
    parser.add_argument("--learning-rate", type=float, default=2e-4)
    parser.add_argument("--lora-r", type=int, default=16)
    parser.add_argument("--lora-alpha", type=int, default=32)
    parser.add_argument("--lora-dropout", type=float, default=0.05)
    parser.add_argument(
        "--target-modules",
        default="q_proj,k_proj,v_proj,o_proj",
        help="comma-separated module names, or 'auto' to let PEFT infer",
    )
    parser.add_argument("--bf16", action="store_true", help="use bfloat16")
    parser.add_argument("--fp16", action="store_true", help="use float16")
    parser.add_argument("--trust-remote-code", action="store_true")
    return parser.parse_args()


def resolve_files(data: str) -> tuple[Path, Path | None]:
    path = Path(data)
    if path.is_file():
        return path, None
    train = path / "train.jsonl"
    if not train.exists():
        raise SystemExit(f"missing {train}; run python3 TRAIN_IA_MODEL/build.py first")
    eval_path = path / "eval.jsonl"
    return train, eval_path if eval_path.exists() else None


def main() -> int:
    args = parse_args()

    import torch
    from datasets import load_dataset
    from peft import LoraConfig, get_peft_model
    from transformers import (
        AutoModelForCausalLM,
        AutoTokenizer,
        DataCollatorForLanguageModeling,
        Trainer,
        TrainingArguments,
    )

    train_file, eval_file = resolve_files(args.data)
    data_files = {"train": str(train_file)}
    if eval_file:
        data_files["validation"] = str(eval_file)

    raw = load_dataset("json", data_files=data_files)
    tokenizer = AutoTokenizer.from_pretrained(
        args.model, trust_remote_code=args.trust_remote_code
    )
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token
    tokenizer.padding_side = "right"

    def tokenize(example: dict) -> dict:
        text = tokenizer.apply_chat_template(
            example["messages"], tokenize=False, add_generation_prompt=False
        )
        return tokenizer(text, truncation=True, max_length=args.max_length, padding=False)

    remove_columns = raw["train"].column_names
    tokenized = raw.map(tokenize, remove_columns=remove_columns)

    dtype = torch.bfloat16 if args.bf16 else torch.float16 if args.fp16 else None
    model = AutoModelForCausalLM.from_pretrained(
        args.model,
        torch_dtype=dtype,
        device_map="auto",
        trust_remote_code=args.trust_remote_code,
    )
    model.config.use_cache = False
    model.config.pad_token_id = tokenizer.pad_token_id

    target_modules = None
    if args.target_modules.strip().lower() != "auto":
        target_modules = [x.strip() for x in args.target_modules.split(",") if x.strip()]

    peft_config = LoraConfig(
        r=args.lora_r,
        lora_alpha=args.lora_alpha,
        lora_dropout=args.lora_dropout,
        bias="none",
        task_type="CAUSAL_LM",
        target_modules=target_modules,
    )
    model = get_peft_model(model, peft_config)
    model.print_trainable_parameters()

    common = dict(
        output_dir=args.output,
        num_train_epochs=args.epochs,
        per_device_train_batch_size=args.batch_size,
        gradient_accumulation_steps=args.grad_accum,
        learning_rate=args.learning_rate,
        logging_steps=10,
        save_strategy="epoch",
        report_to="none",
        remove_unused_columns=False,
        gradient_checkpointing=True,
        bf16=args.bf16,
        fp16=args.fp16,
    )
    eval_strategy = "epoch" if eval_file else "no"
    try:
        training_args = TrainingArguments(**common, eval_strategy=eval_strategy)
    except TypeError:
        training_args = TrainingArguments(**common, evaluation_strategy=eval_strategy)

    trainer = Trainer(
        model=model,
        args=training_args,
        train_dataset=tokenized["train"],
        eval_dataset=tokenized.get("validation"),
        data_collator=DataCollatorForLanguageModeling(tokenizer=tokenizer, mlm=False),
    )
    trainer.train()
    trainer.save_model(args.output)
    tokenizer.save_pretrained(args.output)

    metadata = {
        "base_model": args.model,
        "train_file": str(train_file),
        "eval_file": str(eval_file) if eval_file else None,
        "max_length": args.max_length,
        "epochs": args.epochs,
        "lora_r": args.lora_r,
        "lora_alpha": args.lora_alpha,
        "target_modules": target_modules,
    }
    Path(args.output).mkdir(parents=True, exist_ok=True)
    (Path(args.output) / "redbunny_training.json").write_text(
        json.dumps(metadata, indent=2) + "\n", encoding="utf-8"
    )
    print(f"Saved adapter to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
