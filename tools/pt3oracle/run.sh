#!/usr/bin/env bash
# tools/pt3oracle/run.sh -- construit puis lance l'oracle PT3 (voir README.md).
#
# Usage : tools/pt3oracle/run.sh <module.pt3> <trames> <sortie.csv>
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if command -v dotnet >/dev/null 2>&1; then
  :
elif [ -x "$HOME/.dotnet/dotnet" ]; then
  export PATH="$HOME/.dotnet:$PATH"
else
  echo "MANQUANT : dotnet (.NET 8 SDK)." >&2
  echo "Installation locale, sans root : voir README.md de ce dossier." >&2
  exit 1
fi
export DOTNET_CLI_TELEMETRY_OPTOUT=1

dotnet run --project "$DIR" --configuration Release -- "$@"
