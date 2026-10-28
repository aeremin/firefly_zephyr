# Sets up the Pigweed environment of this project (configured by pigweed.json
# in this folder) in third_party/pigweed/environment and activates it in the
# current shell. Must be sourced, e.g. from the repository root:
#
#   source pigweed_env/bootstrap.sh
#
# Use activate.sh to only activate an already set up environment.
# Based on third_party/pigweed/bootstrap.sh, which projects are meant to copy.

_FIREFLY_SCRIPT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]}")"
PW_PROJECT_ROOT="$(dirname "$_FIREFLY_SCRIPT")"
PW_ROOT="$(cd "$PW_PROJECT_ROOT/../third_party/pigweed" && pwd)"
PW_ENVIRONMENT_ROOT="$PW_ROOT/environment"
export PW_ROOT
export PW_PROJECT_ROOT
export PW_ENVIRONMENT_ROOT
# Don't run `pw doctor` on activation. It checks Pigweed's own development
# setup: tools from its default package set that tools.json leaves out (e.g.
# clang++ and openocd on Linux), and the `pw` command itself, which can't load
# its plugins once the NCS requirements install setuptools >= 81 (no
# pkg_resources). Builds don't use either, but a failed check makes activation
# return non-zero, which fails CI steps.
export PW_ACTIVATE_SKIP_CHECKS=1

. "$PW_ROOT/pw_env_setup/util.sh"

pw_deactivate
(return 0 2>/dev/null) && _pw_sourced=1 || _pw_sourced=0
pw_eval_sourced "$_pw_sourced" "$_FIREFLY_SCRIPT"
if ! pw_check_root "$PW_ROOT"; then
  return
fi

_PW_ACTUAL_ENVIRONMENT_ROOT="$(pw_get_env_root)"
export _PW_ACTUAL_ENVIRONMENT_ROOT
SETUP_SH="$_PW_ACTUAL_ENVIRONMENT_ROOT/activate.sh"

# activate.sh sets _FIREFLY_ACTIVATE_ONLY; it still bootstraps if there is no
# environment yet.
if [ -z "$_FIREFLY_ACTIVATE_ONLY" ] || [ ! -s "$SETUP_SH" ]; then
  pw_bootstrap --shell-file "$SETUP_SH" --install-dir "$_PW_ACTUAL_ENVIRONMENT_ROOT"
  pw_finalize bootstrap "$SETUP_SH"
else
  pw_activate
  pw_finalize activate "$SETUP_SH"
fi

unset _pw_sourced
unset _FIREFLY_SCRIPT
unset _FIREFLY_ACTIVATE_ONLY
unset SETUP_SH
