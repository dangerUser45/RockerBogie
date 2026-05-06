from pathlib import Path

Import("env")

project_dir = Path(env.subst("$PROJECT_DIR"))
build_dir = Path(env.subst("$BUILD_DIR"))
source_html = project_dir / "src" / "control.html"
generated_dir = build_dir / "generated"
generated_header = generated_dir / "control_html.h"

html = source_html.read_text(encoding="utf-8")
delimiter = "RBHTML"

while f"){delimiter}\"" in html:
    delimiter += "_X"

generated_dir.mkdir(parents=True, exist_ok=True)
generated_header.write_text(
    "#pragma once\n"
    "#include <Arduino.h>\n\n"
    f"static const char CONTROL_HTML[] PROGMEM = R\"{delimiter}("
    f"{html}"
    f"){delimiter}\";\n",
    encoding="utf-8",
)

env.Append(CPPPATH=[str(generated_dir)])
