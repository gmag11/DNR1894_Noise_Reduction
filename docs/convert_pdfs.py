import os
from pathlib import Path
import re
import sys

from docling_core.types.doc import ImageRefMode

from docling.datamodel.base_models import ConversionStatus, InputFormat
from docling.datamodel.pipeline_options import CodeFormulaVlmOptions, PdfPipelineOptions
from docling.datamodel.vlm_engine_options import ApiVlmEngineOptions
from docling.document_converter import DocumentConverter, PdfFormatOption
from docling.models.inference_engines.vlm import VlmEngineType


def load_dotenv(dotenv_path: Path) -> None:
    if not dotenv_path.exists():
        return

    for raw_line in dotenv_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue

        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip()

        if not key or key in os.environ:
            continue

        if len(value) >= 2 and value[0] == value[-1] and value[0] in {'"', "'"}:
            value = value[1:-1]

        os.environ[key] = value


def get_remote_config() -> tuple[dict[str, object] | None, str | None]:
    remote_api_url = os.environ.get("DOCLING_API_URL")
    remote_api_model = os.environ.get("DOCLING_API_MODEL")
    remote_api_key = os.environ.get("DOCLING_API_KEY")

    if remote_api_url and remote_api_model:
        headers = {}
        if remote_api_key:
            headers["Authorization"] = f"Bearer {remote_api_key}"

        return (
            {
                "url": remote_api_url,
                "headers": headers,
                "params": {
                    "model": remote_api_model,
                    "max_tokens": 2048,
                },
                "timeout": float(os.environ.get("DOCLING_API_TIMEOUT", "120")),
                "concurrency": int(os.environ.get("DOCLING_API_CONCURRENCY", "1")),
            },
            f"custom API model {remote_api_model}",
        )

    openrouter_api_key = os.environ.get("OPENROUTER_API_KEY")
    openrouter_model = os.environ.get("OPENROUTER_MODEL")
    if openrouter_api_key and openrouter_model:
        headers = {
            "Authorization": f"Bearer {openrouter_api_key}",
            "HTTP-Referer": os.environ.get("OPENROUTER_SITE_URL", "https://localhost"),
            "X-Title": os.environ.get("OPENROUTER_APP_NAME", "docling-pdf-converter"),
        }

        return (
            {
                "url": os.environ.get(
                    "OPENROUTER_API_URL",
                    "https://openrouter.ai/api/v1/chat/completions",
                ),
                "headers": headers,
                "params": {
                    "model": openrouter_model,
                    "max_tokens": int(os.environ.get("OPENROUTER_MAX_TOKENS", "2048")),
                },
                "timeout": float(os.environ.get("OPENROUTER_TIMEOUT", "180")),
                "concurrency": int(os.environ.get("OPENROUTER_CONCURRENCY", "1")),
            },
            f"OpenRouter model {openrouter_model}",
        )

    return None, None


def sanitize_formula_text(text: str) -> str:
    normalized = text.replace("\r\n", "\n").replace("\r", "\n").strip()
    normalized = normalized.replace("\u0332", "").replace("\ufeff", "")
    normalized = re.sub(r"[\u200b-\u200f\u2060]", "", normalized)

    if not normalized:
        return normalized

    display_blocks = re.findall(r"\$\$(.+?)\$\$", normalized, flags=re.DOTALL)
    if display_blocks:
        candidates = [block.strip() for block in display_blocks if block.strip()]
        for candidate in candidates:
            if "\\" in candidate or "=" in candidate:
                return candidate
        return candidates[0]

    inline_blocks = re.findall(r"\$(.+?)\$", normalized, flags=re.DOTALL)
    if inline_blocks:
        candidates = [block.strip() for block in inline_blocks if block.strip()]
        for candidate in candidates:
            if "\\" in candidate or "=" in candidate:
                return candidate
        return candidates[0]

    lines = [line.strip() for line in normalized.split("\n") if line.strip()]
    formula_lines = []
    for line in lines:
        lower_line = line.lower()
        if lower_line.startswith("based on the image"):
            continue
        if lower_line.startswith("the image shows"):
            continue
        if lower_line.startswith("this equation"):
            continue
        if "for the specific case mentioned" in lower_line:
            continue
        formula_lines.append(line.replace("$", ""))

    for line in formula_lines:
        if "=" in line or "\\" in line:
            return line.strip()

    return normalized.replace("$", "").strip()


def sanitize_formulas(document) -> None:
    for item, _ in document.iterate_items():
        text = getattr(item, "text", None)
        if not text or type(item).__name__ != "FormulaItem":
            continue

        item.text = sanitize_formula_text(text)


def relativize_markdown_asset_paths(markdown_path: Path) -> None:
    content = markdown_path.read_text(encoding="utf-8")

    def replace_link(match: re.Match[str]) -> str:
        prefix, target, suffix = match.groups()
        if "://" in target or not os.path.isabs(target):
            return match.group(0)

        relative_target = os.path.relpath(target, markdown_path.parent)
        relative_target = relative_target.replace("\\", "/")
        return f"{prefix}{relative_target}{suffix}"

    updated = re.sub(r"(!?\[[^\]]*\]\()([^)]+)(\))", replace_link, content)
    if updated != content:
        markdown_path.write_text(updated, encoding="utf-8")


def build_converter() -> DocumentConverter:
    pipeline_options = PdfPipelineOptions()
    pipeline_options.generate_page_images = True
    pipeline_options.generate_picture_images = True
    pipeline_options.images_scale = 2.0
    pipeline_options.do_ocr = False

    remote_config, _ = get_remote_config()
    if remote_config:
        pipeline_options.enable_remote_services = True
        pipeline_options.do_formula_enrichment = True
        pipeline_options.code_formula_options = CodeFormulaVlmOptions.from_preset(
            "codeformulav2",
            engine_options=ApiVlmEngineOptions(
                engine_type=VlmEngineType.API,
                url=remote_config["url"],
                headers=remote_config["headers"],
                params=remote_config["params"],
                timeout=remote_config["timeout"],
                concurrency=remote_config["concurrency"],
            ),
        )

    return DocumentConverter(
        format_options={
            InputFormat.PDF: PdfFormatOption(pipeline_options=pipeline_options)
        }
    )


def convert_pdf(converter: DocumentConverter, pdf_path: Path, output_dir: Path) -> None:
    result = converter.convert(pdf_path)

    if result.status != ConversionStatus.SUCCESS:
        raise RuntimeError(f"Conversion failed for {pdf_path.name}: {result.status}")

    sanitize_formulas(result.document)

    output_dir.mkdir(parents=True, exist_ok=True)
    markdown_path = output_dir / f"{pdf_path.stem}.md"
    result.document.save_as_markdown(markdown_path, image_mode=ImageRefMode.REFERENCED)
    relativize_markdown_asset_paths(markdown_path)
    print(f"Created {markdown_path}")


def main() -> int:
    base_dir = Path.cwd()
    output_dir = base_dir / "markdown_output"
    load_dotenv(base_dir / ".env")

    if len(sys.argv) > 1:
        pdf_paths = [Path(arg).resolve() for arg in sys.argv[1:]]
    else:
        pdf_paths = sorted(base_dir.glob("*.pdf"))

    if not pdf_paths:
        print("No PDF files found.", file=sys.stderr)
        return 1

    converter = build_converter()

    _, remote_backend_label = get_remote_config()
    if remote_backend_label:
        print(f"Remote formula enrichment enabled via {remote_backend_label}.")
    else:
        print("Remote formula enrichment disabled.")

    for pdf_path in pdf_paths:
        convert_pdf(converter, pdf_path, output_dir)

    print(f"Output directory: {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())