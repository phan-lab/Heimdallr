/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "world.h"
#include "ns3/core-module.h"

#include <cassert>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Main");

int main(int argc, char *argv[])
{
    LogComponentEnable("TrainApp", LOG_LEVEL_WARN);
    LogComponentEnable("StationApp", LOG_LEVEL_WARN);
    LogComponentEnable("GeneralApp", LOG_LEVEL_WARN);
    LogComponentEnable("Reputation", LOG_LEVEL_WARN);
    // LogComponentEnable("TrainApp", LOG_LEVEL_ALL);
    // LogComponentEnable("StationApp", LOG_LEVEL_ALL);
    // LogComponentEnable("GeneralApp", LOG_LEVEL_ALL);
    // LogComponentEnable("Reputation", LOG_LEVEL_ALL);

    srand(time(0));
    NS_LOG_UNCOND("Starting main");

    World world;

    CommandLine cmd;
    std::string inter_region_config = "";
    std::string config_dir = "";

    cmd.AddValue("config", "Name of the inter-region configuration file", inter_region_config);
    cmd.AddValue("config_dir", "Name of the task configuration directory", config_dir);
    cmd.Parse(argc, argv);
    set_prefix(config_dir);

    // Config::SetDefault ("ns3::LteUeNetDevice::HarqBufferSize", StringValue("1GB"));
    // Config::SetDefault ("ns3::LteEnbNetDevice::HarqBufferSize", StringValue("1GB"));




    world.read_config(inter_region_config);
    world.init_schedule(config_dir + "/scheduling-file-mapping.txt");
    world.start_simulation();
    // world.setup_lte();
    return 0;
}
