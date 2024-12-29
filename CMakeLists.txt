
# Copyright (c) <current-year> <your-organization>
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

add_vpp_plugin(bgp
  SOURCES
  bgp.c
  node.c
  bgp_periodic.c
  bgp_cli.c
  bgp_message_handlers.c
  bgp_neighbors.c
  bgp_prefix_list.c
  bgp_routes.c
  bgp_socket.c
  bgp_state_machine.c
  bgp_utils.c

  MULTIARCH_SOURCES
  node.c

  API_FILES
  bgp.api

  API_TEST_SOURCES
  bgp_test.c

  INSTALL_HEADERS
  bgp.h
)