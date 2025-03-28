# SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: CC0-1.0

import pytest
from pytest_embedded import Dut

@pytest.mark.esp32
@pytest.mark.esp32s2
@pytest.mark.esp32s3
@pytest.mark.esp32p4
@pytest.mark.esp32c2
@pytest.mark.esp32c3
@pytest.mark.esp32c5
@pytest.mark.esp32c6
def test_esp_system(dut: Dut) -> None:
    dut.run_all_single_board_cases()
