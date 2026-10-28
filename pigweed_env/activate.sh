# Activates the Pigweed environment of this project in the current shell (sets
# it up first if needed, see bootstrap.sh). Must be sourced, e.g. from the
# repository root:
#
#   source pigweed_env/activate.sh

_FIREFLY_ACTIVATE_ONLY=1
. "$(dirname "${BASH_SOURCE[0]}")/bootstrap.sh"
