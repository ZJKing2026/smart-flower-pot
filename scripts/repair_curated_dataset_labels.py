"""修复植物养护数据集中的损坏文本标签。"""

from __future__ import annotations

import argparse
import json
import shutil
import tempfile
import zipfile
from pathlib import Path
from typing import Any


SYSTEM_PROMPT = (
    "你是植境的植物视觉分析模型。只能根据图片中可见内容判断，"
    "不得编造土壤湿度、根系状态、缺肥原因或不可见病因。"
    "必须输出合法JSON，不输出额外文字。"
)
USER_PROMPT = (
    "<image>请判断图片主体是否为需要养护的植物，"
    "识别可见健康状态并返回结构化JSON。"
)
SPLITS = ("train", "validation", "test")


def parse_args() -> argparse.Namespace:
    """解析命令行参数并返回配置。"""
    parser = argparse.ArgumentParser(
        description="修复 curated_v1 数据集的文本标签并生成云端更新包。",
    )
    parser.add_argument(
        "dataset_dir",
        type=Path,
        help="curated_v1 数据集目录",
    )
    parser.add_argument(
        "--package",
        type=Path,
        required=True,
        help="云端标签修复包输出路径",
    )
    return parser.parse_args()


def build_visible_evidence(record: dict[str, Any]) -> list[str]:
    """根据主体和健康状态重建可见证据文本。"""
    subject = record["subject"]
    health_status = tuple(record.get("health_status", []))

    if subject == "NON_PLANT":
        return ["图片主体不是需要养护的植物。"]

    if subject == "UNCERTAIN":
        if record.get("source_dataset") == "Pexels review":
            return ["植物主体距离较远，细节不足，无法可靠判断健康状态。"]
        return ["图像质量不足，无法可靠判断主体及健康状态。"]

    evidence_by_status = {
        ("HEALTHY",): "叶片整体颜色正常，未见明显斑点或大面积枯黄。",
        ("LEAF_SPOT",): "叶片可见斑点或局部异常变色。",
        (
            "DRY_LEAF",
            "LEAF_SPOT",
        ): "叶片可见斑点，并伴有干枯或坏死区域。",
        (
            "YELLOW_LEAF",
            "UNKNOWN_ABNORMALITY",
        ): "叶片可见明显黄化及异常纹理。",
    }

    try:
        return [evidence_by_status[health_status]]
    except KeyError as error:
        raise ValueError(
            f"不支持的健康状态组合：{record.get('id')} {health_status}",
        ) from error


def repair_record(record: dict[str, Any]) -> dict[str, Any]:
    """复制记录并修复其中的损坏文本字段。"""
    repaired = dict(record)
    repaired["visible_evidence"] = build_visible_evidence(repaired)

    if repaired["subject"] == "UNCERTAIN":
        if repaired.get("source_dataset") == "Pexels review":
            repaired["source_label"] = "distant plant"
        else:
            repaired["source_label"] = "controlled image degradation"

    return repaired


def load_jsonl(path: Path) -> list[dict[str, Any]]:
    """读取 JSONL 文件并返回记录列表。"""
    records = []
    with path.open("r", encoding="utf-8") as file:
        for line_number, line in enumerate(file, start=1):
            try:
                records.append(json.loads(line))
            except json.JSONDecodeError as error:
                raise ValueError(
                    f"{path}:{line_number} 不是合法JSON",
                ) from error
    return records


def write_jsonl_atomic(path: Path, records: list[dict[str, Any]]) -> None:
    """以 UTF-8 编码原子写入 JSONL 文件。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        "w",
        encoding="utf-8",
        newline="\n",
        dir=path.parent,
        delete=False,
    ) as temporary_file:
        temporary_path = Path(temporary_file.name)
        for record in records:
            temporary_file.write(
                json.dumps(record, ensure_ascii=False) + "\n",
            )

    temporary_path.replace(path)


def to_sharegpt(record: dict[str, Any]) -> dict[str, Any]:
    """将单条结构化记录转换为多模态 ShareGPT 格式。"""
    response = {
        "subject": record["subject"],
        "plant_name": record.get("plant_name"),
        "health_status": record.get("health_status", []),
        "visible_evidence": record["visible_evidence"],
        "confidence": record["confidence"],
        "need_rag": record["need_rag"],
    }
    return {
        "messages": [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": USER_PROMPT},
            {
                "role": "assistant",
                "content": json.dumps(
                    response,
                    ensure_ascii=False,
                    separators=(",", ":"),
                ),
            },
        ],
        "images": [record["image"]],
    }


def build_dataset_info() -> dict[str, Any]:
    """生成 LLaMA Factory 使用的数据集注册配置。"""
    dataset_info = {}
    for split in SPLITS:
        dataset_info[f"plant_care_{split}"] = {
            "file_name": f"{split}_sharegpt.jsonl",
            "formatting": "sharegpt",
            "columns": {
                "messages": "messages",
                "images": "images",
            },
            "tags": {
                "role_tag": "role",
                "content_tag": "content",
                "user_tag": "user",
                "assistant_tag": "assistant",
                "system_tag": "system",
            },
        }
    return dataset_info


def write_json_atomic(path: Path, value: dict[str, Any]) -> None:
    """以 UTF-8 编码原子写入格式化 JSON 文件。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        "w",
        encoding="utf-8",
        newline="\n",
        dir=path.parent,
        delete=False,
    ) as temporary_file:
        temporary_path = Path(temporary_file.name)
        json.dump(value, temporary_file, ensure_ascii=False, indent=2)
        temporary_file.write("\n")

    temporary_path.replace(path)


def create_backup(
    dataset_dir: Path,
    source_files: list[Path],
) -> Path:
    """备份修复前的文本文件并返回备份目录。"""
    backup_dir = dataset_dir / "backup_before_label_repair"
    backup_dir.mkdir(exist_ok=False)
    for source_file in source_files:
        shutil.copy2(source_file, backup_dir / source_file.name)
    return backup_dir


def validate_records(
    dataset_dir: Path,
    records: list[dict[str, Any]],
) -> None:
    """验证修复后的标签、图片路径和必要字段。"""
    required_fields = {
        "id",
        "image",
        "subject",
        "health_status",
        "visible_evidence",
        "confidence",
        "need_rag",
        "split",
    }

    for record in records:
        missing_fields = required_fields - record.keys()
        if missing_fields:
            raise ValueError(
                f"{record.get('id')} 缺少字段：{sorted(missing_fields)}",
            )

        evidence = record["visible_evidence"]
        if not evidence or any("?" in text for text in evidence):
            raise ValueError(f"{record['id']} 的可见证据仍然损坏")

        image_path = dataset_dir / record["image"]
        if not image_path.is_file():
            raise FileNotFoundError(f"图片不存在：{image_path}")


def create_repair_package(
    dataset_dir: Path,
    package_path: Path,
) -> None:
    """打包云端需要替换的标签和配置文件。"""
    package_files = [
        dataset_dir / "dataset.jsonl",
        *(dataset_dir / f"{split}.jsonl" for split in SPLITS),
        *(dataset_dir / f"{split}_sharegpt.jsonl" for split in SPLITS),
        dataset_dir / "dataset_info.json",
    ]

    package_path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(
        package_path,
        "w",
        compression=zipfile.ZIP_DEFLATED,
        compresslevel=9,
    ) as archive:
        for file_path in package_files:
            archive.write(file_path, arcname=file_path.name)


def main() -> None:
    """执行数据修复、转换、验证、备份和打包。"""
    args = parse_args()
    dataset_dir = args.dataset_dir.resolve()

    source_paths = [
        dataset_dir / "dataset.jsonl",
        *(dataset_dir / f"{split}.jsonl" for split in SPLITS),
    ]
    for source_path in source_paths:
        if not source_path.is_file():
            raise FileNotFoundError(f"缺少数据文件：{source_path}")

    backup_dir = create_backup(dataset_dir, source_paths)

    repaired_by_split: dict[str, list[dict[str, Any]]] = {}
    for split in SPLITS:
        records = load_jsonl(dataset_dir / f"{split}.jsonl")
        repaired_by_split[split] = [repair_record(record) for record in records]

    all_records = [
        record
        for split in SPLITS
        for record in repaired_by_split[split]
    ]
    validate_records(dataset_dir, all_records)

    write_jsonl_atomic(dataset_dir / "dataset.jsonl", all_records)
    for split, records in repaired_by_split.items():
        write_jsonl_atomic(dataset_dir / f"{split}.jsonl", records)
        write_jsonl_atomic(
            dataset_dir / f"{split}_sharegpt.jsonl",
            [to_sharegpt(record) for record in records],
        )

    write_json_atomic(dataset_dir / "dataset_info.json", build_dataset_info())
    create_repair_package(dataset_dir, args.package.resolve())

    print(f"backup_dir: {backup_dir}")
    print(f"dataset_records: {len(all_records)}")
    for split in SPLITS:
        print(f"{split}_records: {len(repaired_by_split[split])}")
    print(f"repair_package: {args.package.resolve()}")
    print("LABEL_REPAIR_OK")


if __name__ == "__main__":
    main()
