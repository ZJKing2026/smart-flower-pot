"""规范植物养护数据集中的物种名称。"""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path
from typing import Any

from repair_curated_dataset_labels import (
    SPLITS,
    build_dataset_info,
    create_repair_package,
    load_jsonl,
    to_sharegpt,
    validate_records,
    write_json_atomic,
    write_jsonl_atomic,
)


PLANT_NAME_MAPPING = {
    "Corn Gray": "Corn",
    "Potato Early Blight": "Potato",
    "Squash Powdery Mildew": "Squash",
    "Tomato Early Blight": "Tomato",
    "Tomato Septoria": "Tomato",
}


def parse_args() -> argparse.Namespace:
    """解析命令行参数并返回配置。"""
    parser = argparse.ArgumentParser(
        description="规范 curated_v1 数据集中的植物名称。",
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
        help="云端更新包输出路径",
    )
    return parser.parse_args()


def normalize_record(
    record: dict[str, Any],
) -> tuple[dict[str, Any], bool]:
    """复制记录并规范其中的植物名称。"""
    normalized = dict(record)
    current_name = normalized.get("plant_name")

    if current_name not in PLANT_NAME_MAPPING:
        return normalized, False

    normalized["plant_name"] = PLANT_NAME_MAPPING[current_name]
    return normalized, True


def create_backup(
    dataset_dir: Path,
    source_files: list[Path],
) -> Path:
    """备份名称规范化前的文本文件。"""
    backup_dir = dataset_dir / "backup_before_plant_name_normalization"
    backup_dir.mkdir(exist_ok=False)

    for source_file in source_files:
        shutil.copy2(source_file, backup_dir / source_file.name)

    return backup_dir


def main() -> None:
    """执行名称规范化、验证、备份和打包。"""
    args = parse_args()
    dataset_dir = args.dataset_dir.resolve()

    source_files = [
        dataset_dir / "dataset.jsonl",
        *(dataset_dir / f"{split}.jsonl" for split in SPLITS),
        *(dataset_dir / f"{split}_sharegpt.jsonl" for split in SPLITS),
        dataset_dir / "dataset_info.json",
    ]
    for source_file in source_files:
        if not source_file.is_file():
            raise FileNotFoundError(f"缺少数据文件：{source_file}")

    backup_dir = create_backup(dataset_dir, source_files)
    normalized_by_split: dict[str, list[dict[str, Any]]] = {}
    changed_count = 0

    for split in SPLITS:
        normalized_records = []
        for record in load_jsonl(dataset_dir / f"{split}.jsonl"):
            normalized, changed = normalize_record(record)
            normalized_records.append(normalized)
            changed_count += int(changed)

        normalized_by_split[split] = normalized_records

    if changed_count != 75:
        raise ValueError(
            f"预期规范化75条记录，实际为{changed_count}条",
        )

    all_records = [
        record
        for split in SPLITS
        for record in normalized_by_split[split]
    ]
    validate_records(dataset_dir, all_records)

    write_jsonl_atomic(dataset_dir / "dataset.jsonl", all_records)
    for split, records in normalized_by_split.items():
        write_jsonl_atomic(dataset_dir / f"{split}.jsonl", records)
        write_jsonl_atomic(
            dataset_dir / f"{split}_sharegpt.jsonl",
            [to_sharegpt(record) for record in records],
        )

    write_json_atomic(dataset_dir / "dataset_info.json", build_dataset_info())
    create_repair_package(dataset_dir, args.package.resolve())

    print(f"backup_dir: {backup_dir}")
    print(f"normalized_records: {changed_count}")
    print(f"dataset_records: {len(all_records)}")
    print(f"update_package: {args.package.resolve()}")
    print("PLANT_NAME_NORMALIZATION_OK")


if __name__ == "__main__":
    main()
