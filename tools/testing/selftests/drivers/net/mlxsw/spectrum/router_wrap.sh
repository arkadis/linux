#!/bin/bash
# SPDX-License-Identifier: GPL-2.0

NUM_NETIFS=4
source lib.sh
source tc_common.sh

relative_path="${BASH_SOURCE%/*}"
if [[ "$relative_path" == "${BASH_SOURCE}" ]]; then
	relative_path="."
fi

source "$relative_path/router_scale.sh"

trap router_cleanup EXIT

router_setup_prepare
setup_wait
router_test 100 0
log_test "router test"

exit $EXIT_STATUS
