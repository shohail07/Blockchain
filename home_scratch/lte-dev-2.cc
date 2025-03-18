
// NS_LOG="Ipv4L3Protocol" ./ns3 run lte-dev-2
// ./ns3 run lte-dev-2




#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/waypoint-mobility-model.h"
#include "ns3/waypoint.h"
#include "ns3/log.h"

#include <sstream>

using namespace ns3;



int main(int argc, char* argv[])
{

/* 1. LOG START */
	
	LogComponentEnable("LteEnbRrc", LOG_LEVEL_INFO);
	LogComponentEnable("LteUeRrc", LOG_LEVEL_INFO);
	
	//LogComponentEnable("LteEnbRrc", LOG_LEVEL_FUNCTION);
	//LogComponentEnable("LteUeRrc", LOG_LEVEL_FUNCTION);
	
/* 1. LOG END */
    
/* 2. Node Creation START 
eNodeBs (Evolved Node Bs)
User Equipments (UEs)
*/
	// Create the node containers for the LTE network components
	NodeContainer enbNodes;
	enbNodes.Create(2); // Two eNodeBs (towers)
	NodeContainer ueNodes;
	ueNodes.Create(10); // Ten UEs
/* 2. Node Creation END */


/* 3.Evolved Packet Core (EPC): START */
    
	// Create a PointToPointEpcHelper for the Evolved Packet Core
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    // Create an LTE helper and attach the EPC helper to it
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetEpcHelper(epcHelper);
	
/* 3.Evolved Packet Core (EPC): END */

/* 4. Mobility Models: START */
	
    // Mobility setup for eNodeBs
    MobilityHelper enodeB_mobility;
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(250, 500, 0)); // Tower 1 position
    enbPositionAlloc->Add(Vector(750, 500, 0)); // Tower 2 position
    enodeB_mobility.SetPositionAllocator(enbPositionAlloc);
    enodeB_mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enodeB_mobility.Install(enbNodes);

	// Mobility setup for UEs
	MobilityHelper ueMobility;

	// Create a position allocator and assign positions to UEs around each tower
	Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator>();
	for (int i = 0; i < 5; ++i) {
		// Positions for UEs around Tower 1
		uePositionAlloc->Add(Vector(200 + i * 20, 450 - i * 15, 0));
	}
	for (int i = 5; i < 10; ++i) {
		// Positions for UEs around Tower 2
		uePositionAlloc->Add(Vector(700 + (i - 5) * 20, 450 - (i - 5) * 15, 0));
	}

	// Set the position allocator for UE mobility
	ueMobility.SetPositionAllocator(uePositionAlloc);
	ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Mode", StringValue("Time"),
                            "Time", TimeValue(Seconds(1)), // How often the direction and speed are updated
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=10]"), // 20 m/s speed
                            "Bounds", RectangleValue(Rectangle(0, 1000, 0, 1000))); // Expanding the area                        

	// Install the mobility model on the UE nodes
	ueMobility.Install(ueNodes);

/* 4. Mobility Models: END */

/* 5. LTE Helper: START */
	
	// Correctly declare and initialize LTE devices for eNodeBs and UEs
    NetDeviceContainer enbLteDevices = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevices = lteHelper->InstallUeDevice(ueNodes);

    // Install the internet stack on all UEs and the remote host (only once)
    InternetStackHelper internet;
    NodeContainer allNodes = NodeContainer(ueNodes, epcHelper->GetPgwNode());
    internet.Install(allNodes);
	
/* 5. LTE Helper: END */
	    

/*
6. Internet Stack:	START
Installed on UEs and the remote host (part of EPC) to enable IP-based communication over the LTE network.
*/
	
    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevices));

    // Attach UEs to the eNodeB
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
        lteHelper->Attach(ueLteDevices.Get(u), enbLteDevices.Get(u / 5));
    }

    /* Setup a Remote Host */
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    
    internet.Install(remoteHostContainer);
/*	6. Internet Stack:	END	*/


/* 7. Point-to-Point (P2P) Links:	START */
    PointToPointHelper p2p;
    // enable packet tracing
    p2p.EnablePcapAll("lte_dev_2/pcap_files/lte-dev-2");
	
	p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", TimeValue(Seconds(0.01)));
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    NetDeviceContainer internetDevices = p2p.Install(pgw, remoteHost);


    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.2.3.0", "255.255.255.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

/* 7. Point-to-Point (P2P) Links:	END */

/* remote host START */

	uint16_t echoPort = 9; // Arbitrary port number for echo service
	UdpEchoServerHelper echoServer(echoPort);

	ApplicationContainer serverApps = echoServer.Install(ueNodes);
	serverApps.Start(Seconds(1.0));
	serverApps.Stop(Seconds(30.0)); // Assuming you want the simulation to run for 10 seconds


	UdpEchoClientHelper echoClient(Ipv4Address::GetAny(), echoPort); // Target address will be set per packet
	echoClient.SetAttribute("MaxPackets", UintegerValue(1));
	echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Send packet every 1 second
	echoClient.SetAttribute("PacketSize", UintegerValue(1024)); // Packet size of 1024 bytes


	/*
	UE node communicates with all other UE nodes (excluding itself) using a UDP echo client. 
	The communication is established through the simulation of UDP echo packets being sent 
	from one UE node to another. 
	*/
	for (uint32_t i = 0; i < ueNodes.GetN(); ++i) 
	{
		for (uint32_t j = 0; j < ueNodes.GetN(); ++j) 
		{
			if (i != j) 
			{ 	
				// Ensure a UE does not send packets to itself
				echoClient.SetAttribute("RemoteAddress", AddressValue(ueIpIface.GetAddress(j))); // Set the target address to UE j
				
				// NS_LOG_INFO("Setting up UDP echo client on UE " << i << " to send packets to UE " << j);
				std::cout << "Setting up UDP echo client on UE " << i << " to send packets to UE " << j << std::endl;
				
				ApplicationContainer clientApp = echoClient.Install(ueNodes.Get(i)); // Install client app on UE i
				
				clientApp.Start(Seconds(2.0)); // Start 1 second after servers to avoid packet loss
				
				clientApp.Stop(Seconds(30.0));
			}
		}
	}
    
/* remote host END ------- */
	


/* Install Applications for distinct Tower-UE communication START */


    uint16_t dlPort1 = 1234; // Port for Tower_1 communication
    uint16_t dlPort2 = 1235; // Port for Tower_2 communication

	ApplicationContainer clientAppsTower1, serverAppsTower1;

	// Setup communication for Tower_1 UEs
	for (uint32_t u = 0; u < 5; ++u) 
	{ 
		// Assuming first 5 UEs are for Tower_1
		UdpEchoClientHelper echoClientTower1(ueIpIface.GetAddress(u), dlPort1);
		echoClientTower1.SetAttribute("MaxPackets", UintegerValue(1));
		echoClientTower1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
		echoClientTower1.SetAttribute("PacketSize", UintegerValue(1024));
		clientAppsTower1.Add(echoClientTower1.Install(remoteHost));
	}

	UdpEchoServerHelper echoServerTower1(dlPort1);

	// First UE of Tower_1 as a server
	serverAppsTower1 = echoServerTower1.Install(ueNodes.Get(0)); 
	
	serverAppsTower1.Start(Seconds(1.0));
	clientAppsTower1.Start(Seconds(2.0));
	
	serverAppsTower1.Stop(Seconds(30.0));
	clientAppsTower1.Stop(Seconds(30.0));



	ApplicationContainer clientAppsTower2, serverAppsTower2;
				    		    
	// Setup communication for Tower_2 UEs
	for (uint32_t u = 5; u < 10; ++u) 
	{ 
		// Assuming next 5 UEs are for Tower_2
		UdpEchoClientHelper echoClientTower2(ueIpIface.GetAddress(u), dlPort2);
		echoClientTower2.SetAttribute("MaxPackets", UintegerValue(1));
		echoClientTower2.SetAttribute("Interval", TimeValue(Seconds(1.1))); // a bit offset timing
		echoClientTower2.SetAttribute("PacketSize", UintegerValue(1024));
		clientAppsTower2.Add(echoClientTower2.Install(remoteHost));
	}
	
	UdpEchoServerHelper echoServerTower2(dlPort2);

	// First UE of Tower_2 as a server		
	serverAppsTower2 = echoServerTower2.Install(ueNodes.Get(5)); 

	serverAppsTower2.Start(Seconds(1.5));
	clientAppsTower2.Start(Seconds(2.5));
	
	serverAppsTower2.Stop(Seconds(30.0));
	clientAppsTower2.Stop(Seconds(30.0));

/* Install Applications for distinct Tower-UE communication END */


/* Animation jobs START */

    // Set up the Animation Interface for visualization
    AnimationInterface anim("lte_dev_2/lte-dev-2-anim.xml");
    anim.EnablePacketMetadata(true);
    
    anim.UpdateNodeDescription(remoteHost, "Remote_Host");
    anim.UpdateNodeColor(remoteHost->GetId(),  255, 255, 0 ); // Green for Tower 1
	//    anim.UpdateNodeImage(remoteHost->GetId(), anim.AddResource("/home/tanvir/Documents/ns-allinone-3.41/netanim-3.109/remote_host.jpg"));
    
    
	// Name the eNodeBs
	
	//anim.UpdateNodeImage(enbNodes.Get(0)->GetId(), anim.AddResource("/home/tanvir/Documents/ns-allinone-3.41/netanim-3.109/tower.png"));
	anim.UpdateNodeColor(enbNodes.Get(0), 0, 255, 0); // Green for Tower 1
	anim.UpdateNodeSize(enbNodes.Get(0)->GetId(), 20, 20);

	//anim.UpdateNodeImage(enbNodes.Get(1)->GetId(), anim.AddResource("/home/tanvir/Documents/ns-allinone-3.41/netanim-3.109/tower.png"));
	anim.UpdateNodeColor(enbNodes.Get(1), 0, 0, 255); // Blue for Tower 2
	anim.UpdateNodeSize(enbNodes.Get(1)->GetId(), 20, 20);
	
	 anim.UpdateNodeDescription(enbNodes.Get(0), "Tower_1");
	 anim.UpdateNodeDescription(enbNodes.Get(1), "Tower_2");


    // Naming UE nodes with prefix based on their associated Tower
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
        std::ostringstream os;
        if (u < 5) {
            os << "UE_t1_" << u; // UEs associated with Tower 1
        } else {
            os << "UE_t2_" << u; // UEs associated with Tower 2
        }
        anim.UpdateNodeDescription(ueNodes.Get(u), os.str());
        anim.UpdateNodeColor(ueNodes.Get(u), 255, 0, 0); // Red for UEs
        anim.UpdateNodeImage(ueNodes.Get(u)->GetId(), anim.AddResource("/home/tanvir/Documents/ns-allinone-3.41/netanim-3.109/smartphone.png"));
        anim.UpdateNodeSize(ueNodes.Get(u)->GetId(), 20, 20);
    }

/* Animation jobs END */
	
	
/*	10. Flow Monitor: START	*/	
	// enable FLow Monitor
	Ptr<FlowMonitor> flowMonitor;
	FlowMonitorHelper flowHelper;
	flowMonitor = flowHelper.InstallAll();
/*	10. Flow Monitor: END	*/

/*	11. Simulator Job START */
    // Set the simulation to stop after 30 seconds
    Simulator::Stop(Seconds(10.0));
    // Run the simulation
    Simulator::Run();

	flowMonitor->SerializeToXmlFile("lte_dev_2/lte_flow_monitor.xml", true, true);

    Simulator::Destroy();
/*	11. Simulator Job END */

    return 0;
}

