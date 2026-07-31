"""评估植境 Qwen3-VL LoRA 模型的测试集表现。"""

from __future__ import annotations

import json
from collections import Counter
from pathlib import Path
from typing import Any

import torch
from peft import PeftModel
from PIL import Image
from transformers import (
    AutoProcessor,
    BitsAndBytesConfig,
    Qwen3VLForConditionalGeneration,
)


BASE_DIR = Path("/root/autodl-tmp/plant-care-training")
MODEL_PATH = BASE_DIR / "models/base/Qwen3-VL-4B-Instruct"
ADAPTER_PATH = BASE_DIR / "outputs/checkpoints/formal-v1"
DATASET_DIR = BASE_DIR / "data/curated_v1"
OUTPUT_DIR = BASE_DIR / "outputs/evaluation/formal-v1"
PREDICTIONS_PATH = OUTPUT_DIR / "test_predictions.jsonl"
METRICS_PATH = OUTPUT_DIR / "metrics.json"

SYSTEM_PROMPT = (
    "你是植境的植物视觉分析模型。只能根据图片中可见内容判断，"
    "不得编造土壤湿度、根系状态、缺肥原因或不可见病因。"
    "必须输出合法JSON，不输出额外文字。"
)
USER_PROMPT = (
    "请判断图片主体是否为需要养护的植物，"
    "识别可见健康状态并返回结构化JSON。"
)
REQUIRED_KEYS = {
    "subject",
    "plant_name",
    "health_status",
    "visible_evidence",
    "confidence",
    "need_rag",
}
ALLOWED_SUBJECTS = {"PLANT", "NON_PLANT", "UNCERTAIN"}
ALLOWED_HEALTH_STATUS = {
    "HEALTHY",
    "YELLOW_LEAF",
    "DRY_LEAF",
    "LEAF_SPOT",
    "UNKNOWN_ABNORMALITY",
}


def load_records() -> list[dict[str, Any]]:
    """读取全部测试记录。"""
    with (DATASET_DIR / "test.jsonl").open(
        "r",
        encoding="utf-8",
    ) as file:
        return [json.loads(line) for line in file]


def load_model() -> tuple[PeftModel, Any]:
    """加载 4-bit 基础模型、Adapter 和处理器。"""
    quantization_config = BitsAndBytesConfig(
        load_in_4bit=True,
        bnb_4bit_quant_type="nf4",
        bnb_4bit_compute_dtype=torch.bfloat16,
        bnb_4bit_use_double_quant=True,
    )
    base_model = Qwen3VLForConditionalGeneration.from_pretrained(
        MODEL_PATH,
        quantization_config=quantization_config,
        dtype=torch.bfloat16,
        device_map="auto",
        trust_remote_code=True,
        local_files_only=True,
        low_cpu_mem_usage=True,
    )
    model = PeftModel.from_pretrained(
        base_model,
        ADAPTER_PATH,
        is_trainable=False,
    )
    model.eval()
    processor = AutoProcessor.from_pretrained(
        MODEL_PATH,
        trust_remote_code=True,
        local_files_only=True,
    )
    return model, processor


def generate(
    model: PeftModel,
    processor: Any,
    image_path: Path,
) -> str:
    """对单张图片生成结构化分析结果。"""
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
    return processor.batch_decode(
        trimmed_ids,
        skip_special_tokens=True,
        clean_up_tokenization_spaces=False,
    )[0].strip()


def schema_is_valid(value: Any) -> bool:
    """检查预测是否符合植境 JSON 协议。"""
    if not isinstance(value, dict):
        return False
    if not REQUIRED_KEYS.issubset(value):
        return False
    if value["subject"] not in ALLOWED_SUBJECTS:
        return False
    if value["plant_name"] is not None and not isinstance(
        value["plant_name"],
        str,
    ):
        return False

    health_status = value["health_status"]
    if not isinstance(health_status, list):
        return False
    if not all(
        isinstance(status, str)
        and status in ALLOWED_HEALTH_STATUS
        for status in health_status
    ):
        return False

    evidence = value["visible_evidence"]
    if not isinstance(evidence, list):
        return False
    if not all(
        isinstance(item, str) and item
        for item in evidence
    ):
        return False

    confidence = value["confidence"]
    if isinstance(confidence, bool):
        return False
    if not isinstance(confidence, (int, float)):
        return False
    if not 0 <= confidence <= 1:
        return False
    return isinstance(value["need_rag"], bool)


def normalized_name(value: Any) -> str | None:
    """规范物种名称以便进行大小写无关比较。"""
    if value is None:
        return None
    if not isinstance(value, str):
        return ""
    return " ".join(value.strip().casefold().split())


def rate(numerator: int, denominator: int) -> float:
    """计算比例并处理零分母。"""
    if denominator == 0:
        return 0.0
    return round(numerator / denominator, 6)


def build_metrics(
    total: int,
    counts: Counter[str],
) -> dict[str, Any]:
    """根据计数器生成测试指标。"""
    return {
        "total_samples": total,
        "counts": dict(counts),
        "rates": {
            "generation_success_rate": rate(
                counts["generation_success"],
                total,
            ),
            "json_valid_rate": rate(counts["json_valid"], total),
            "schema_valid_rate": rate(counts["schema_valid"], total),
            "subject_accuracy": rate(
                counts["subject_correct"],
                total,
            ),
            "health_status_exact_accuracy": rate(
                counts["health_status_exact"],
                total,
            ),
            "known_plant_name_accuracy": rate(
                counts["known_plant_name_correct"],
                counts["known_plant_name_total"],
            ),
            "non_plant_rejection_rate": rate(
                counts["non_plant_correct"],
                counts["non_plant_total"],
            ),
            "uncertain_rejection_rate": rate(
                counts["uncertain_correct"],
                counts["uncertain_total"],
            ),
            "need_rag_accuracy": rate(
                counts["need_rag_correct"],
                total,
            ),
            "confidence_valid_rate": rate(
                counts["confidence_valid"],
                total,
            ),
        },
    }


def update_counts(
    counts: Counter[str],
    gold: dict[str, Any],
    prediction: dict[str, Any],
) -> None:
    """根据单条合法预测更新评估计数。"""
    if prediction["subject"] == gold["subject"]:
        counts["subject_correct"] += 1
    if sorted(prediction["health_status"]) == sorted(
        gold["health_status"]
    ):
        counts["health_status_exact"] += 1
    if prediction["need_rag"] == gold["need_rag"]:
        counts["need_rag_correct"] += 1
    if 0 <= prediction["confidence"] <= 1:
        counts["confidence_valid"] += 1

    if (
        gold["subject"] == "PLANT"
        and gold.get("plant_name") is not None
        and normalized_name(prediction["plant_name"])
        == normalized_name(gold["plant_name"])
    ):
        counts["known_plant_name_correct"] += 1
    if (
        gold["subject"] == "NON_PLANT"
        and prediction["subject"] == "NON_PLANT"
    ):
        counts["non_plant_correct"] += 1
    if (
        gold["subject"] == "UNCERTAIN"
        and prediction["subject"] == "UNCERTAIN"
    ):
        counts["uncertain_correct"] += 1


def main() -> None:
    """执行测试集推理并保存逐条结果和汇总指标。"""
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    records = load_records()
    model, processor = load_model()
    counts: Counter[str] = Counter()

    with PREDICTIONS_PATH.open(
        "w",
        encoding="utf-8",
        newline="\n",
    ) as output_file:
        for index, gold in enumerate(records, start=1):
            if gold["subject"] == "NON_PLANT":
                counts["non_plant_total"] += 1
            if gold["subject"] == "UNCERTAIN":
                counts["uncertain_total"] += 1
            if (
                gold["subject"] == "PLANT"
                and gold.get("plant_name") is not None
            ):
                counts["known_plant_name_total"] += 1

            raw_response = ""
            prediction = None
            error_message = None
            try:
                raw_response = generate(
                    model,
                    processor,
                    DATASET_DIR / gold["image"],
                )
                counts["generation_success"] += 1
                prediction = json.loads(raw_response)
                counts["json_valid"] += 1
            except json.JSONDecodeError as error:
                error_message = f"JSONDecodeError: {error}"
            except Exception as error:
                error_message = (
                    f"{type(error).__name__}: {error}"
                )

            valid_schema = schema_is_valid(prediction)
            if valid_schema:
                counts["schema_valid"] += 1
                update_counts(counts, gold, prediction)

            result = {
                "id": gold["id"],
                "image": gold["image"],
                "gold": {
                    "subject": gold["subject"],
                    "plant_name": gold.get("plant_name"),
                    "health_status": gold["health_status"],
                    "visible_evidence": gold["visible_evidence"],
                    "confidence": gold["confidence"],
                    "need_rag": gold["need_rag"],
                },
                "prediction": prediction,
                "raw_response": raw_response,
                "schema_valid": valid_schema,
                "error": error_message,
            }
            output_file.write(
                json.dumps(result, ensure_ascii=False) + "\n"
            )
            output_file.flush()
            print(
                f"[{index:03d}/{len(records):03d}] "
                f"id={gold['id']} "
                f"json={prediction is not None} "
                f"schema={valid_schema}",
                flush=True,
            )

    metrics = build_metrics(len(records), counts)
    METRICS_PATH.write_text(
        json.dumps(metrics, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print("\n=== TEST METRICS ===")
    print(json.dumps(metrics, ensure_ascii=False, indent=2))
    print("FORMAL_V1_TEST_EVALUATION_OK")


if __name__ == "__main__":
    main()
