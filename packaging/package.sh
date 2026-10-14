#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-only
# Packs one system's release zip: the plugin bundles, the Standalone app, the licence and a short
# read-me (the AGPL asks for the licence notice to travel with the program). Run from the
# repository root after a build. Used by the release workflow and, as a dry run, by CI.
#
#   packaging/package.sh <build type> <version> <zip file name>
set -euo pipefail

build_type="$1"
version="$2"
zip_path="$3"

# The zip is written in the current folder (the repository root); a bare name keeps the rm below safe.
case "${zip_path}" in
  */* | *\\* | "" | .* ) echo "The zip must be a plain file name, got '${zip_path}'"; exit 1 ;;
  *.zip) ;;
  *) echo "The zip name must end in .zip, got '${zip_path}'"; exit 1 ;;
esac

artefacts="build/src/plugin/FrenchcoreMachine_artefacts/${build_type}"
stage="package-stage"
rm -rf "${stage}" "${zip_path}"
mkdir -p "${stage}/Standalone"

cp LICENSE "${stage}/LICENSE.txt"
sed "s/{VERSION}/${version}/g" packaging/README.txt > "${stage}/README.txt"
cp -R "${artefacts}/VST3" "${stage}/"
if [ -d "${artefacts}/AU" ]; then cp -R "${artefacts}/AU" "${stage}/"; fi

case "${RUNNER_OS:-$(uname -s)}" in
  Windows | MINGW* | MSYS*)
    # Only the program: the linker's .lib and .exp leftovers stay out.
    cp "${artefacts}/Standalone/Frenchcore Machine.exe" "${stage}/Standalone/"
    ;;
  *)
    cp -R "${artefacts}/Standalone/." "${stage}/Standalone/"
    ;;
esac

# The Linux binaries keep their symbol tables after a Release build; they are not needed to run.
case "${RUNNER_OS:-$(uname -s)}" in
  Linux)
    find "${stage}" -type f \( -name '*.so' -o -name 'Frenchcore Machine' \) -exec strip --strip-unneeded {} +
    ;;
esac

(
  cd "${stage}"
  case "${RUNNER_OS:-$(uname -s)}" in
    Windows | MINGW* | MSYS*) 7z a -tzip "../${zip_path}" ./* > /dev/null ;;
    # -y keeps the symbolic links inside macOS bundles.
    *) zip -q -r -y "../${zip_path}" . ;;
  esac
)
rm -rf "${stage}"
echo "Packed ${zip_path}"
