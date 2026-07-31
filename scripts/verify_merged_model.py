"""验证植境合并模型的单图推理能力。"""

from __future__ import annotations

import json
from pathlib import Path

import torch
from PIL import Image
from transformers import (
    AutoProcessor,
    Qwen3VLForConditionalGeneration,
)


BASE_DIR = Path("/root/autodl-tmp/plant-care-training")
MODEL_PATH = BASE_DIR / "models/merged/formal-v1"
DATASET_DIR = BASE_DIR / "data/curated_v1"

SYSTEM_PROMPT = (
    "你是植境的植物视觉分析模型。只能根据图片中可见内容判断，"
    "不得编造土壤湿度、根系状态、缺肥原因或不可见病因。"
    "必须输出合法JSON，不输出额外文字。"
)
USER_PROMPT = (
    "请判断图片主体是否为需要养护的植物，"
    "识别可见健康状态并返回结构化JSON。"
)


def load_sample() -> dict:
    """读取测试集第一条记录。"""
    with (DATASET_DIR / "test.jsonl").open(
        "r",
        encoding="utf-8",
    ) as file:
        return json.loads(file.readline())


def main() -> None:
    """加载合并模型并执行单张图片推理。"""
    sample = load_sample()
    image_path = DATASET_DIR / sample["image"]

    model = Qwen3VLForConditionalGeneration.from_pretrained(
        MODEL_PATH,
        dtype=torch.bfloat16,
        device_map="auto",
        trust_remote_code=True,
        local_files_only=True,
        low_cpu_mem_usage=True,
    )
    model.eval()
    processor = AutoProcessor.from_pretrained(
        MODEL_PATH,
        trust_remote_code=True,
        local_files_only=True,
    )

    with Image.open(image_path) as source_image:
        image = source_image.convert("RGB")

    messages = [
        {
            "role": "system",
            "content": [{"type": "text", "text": SYSTEM_PROMPT}],
        },
        {
            "role": "user",
            "content": [
                {"type": "image", "image": image},
                {"type": "text", "text": USER_PROMPT},
            ],
        },
    ]
    inputs = processor.apply_chat_template(
        messages,
        tokenize=True,
        add_generation_prompt=True,
        return_dict=True,
        return_tensors="pt",
    ).to(model.device)

    with torch.inference_mode():
        output_ids = model.generate(
            **inputs,
            max_new_tokens=192,
            do_sample=False,
        )

    trimmed_ids = [
        output[len(prompt):]
        for prompt, output in zip(
            inputs.input_ids,
            output_ids,
            strict=True,
        )
    ]
    response = processor.batch_decode(
        trimmed_ids,
        skip_special_tokens=True,
        clean_up_tokenization_spaces=False,
    )[0].strip()
    prediction = json.loads(response)

    expected = {
        "subject": sample["subject"],
        "plant_name": sample.get("plant_name"),
        "health_status": sample["health_status"],
        "visible_evidence": sample["visible_evidence"],
        "confidence": sample["confidence"],
        "need_rag": sample["need_rag"],
    }
    print("image:", image_path)
    print("expected:", json.dumps(expected, ensure_ascii=False))
    print("prediction:", json.dumps(prediction, ensure_ascii=False))
    print(
        "gpu_peak_gb:",
        round(torch.cuda.max_memory_allocated(0) / 1024**3, 2),
    )
    print("MERGED_MODEL_INFERENCE_OK")


if __name__ == "__main__":
    main()
