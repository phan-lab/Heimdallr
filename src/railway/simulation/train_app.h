#ifndef _TRAIN_APP_H_
#define _TRAIN_APP_H_

#include "general_app.h"

#include "ns3/core-module.h"
#include "ns3/mobility-module.h"

#if ENABLE_TRACK_LOG
#include <fstream>
#endif

namespace ns3
{
  class TrainApp : public GeneralApp
  {
  protected:
    virtual void StartApplication();
    Ptr<ConstantAccelerationMobilityModel> mobility_model;

#if USE_TCP
    std::map<NodeInfo, Ptr<Socket>> m_connections;
    void ConnectionSucceeded(Ptr<Socket> socket);
    void ConnectionFailed(Ptr<Socket> socket);
    void SetupConnections();
    virtual void SendLteMessage(const NodeInfo &dest, const GeneralMessage &msg);
    void SendLteMessageReal(const NodeInfo &dest, const GeneralMessage &msg);
#endif

#if ENABLE_TRACK_LOG
    std::ofstream track_log_ofs;
#endif

    void ProcessMessage(const GeneralMessage &msg, const NodeInfo &src);

    /* Application level local data */
    double position, speed, acc;                //!< If necessary, store the data collected by the sensor
    uint8_t sig_of_sensor[MULTISIG_SIG_LENGTH]; //!< Store the signature of the sensor as well

    double ma_location, ma_speed;
    uint8_t ma_sig[MULTISIG_SIG_LENGTH];

    double target_speed;      //!< stored by the actuator
    int64_t target_speed_seq; //!< stored by the actuator as the latest seq

    /* Application level functions */
    void SensorCollectsData();
    void Actuate();
    void ProcessSensorData(const GeneralMessage &msg, const NodeInfo &src);
    void ControllerUploadLocationSpeed();
    void ProcessControlUpdateLocal(const GeneralMessage &msg, const NodeInfo &src);
    void ProcessMA(const GeneralMessage &msg, const NodeInfo &src);
    void ProcessMAFwd(const GeneralMessage &msg, const NodeInfo &src);

    /* The task that computes the target speed and sends to the actuator */
    void SendControlInput();
    void ProcessControlInput(const GeneralMessage &msg, const NodeInfo &src);
    double CalcTargetSpeed(double front_speed, double front_loc);
    void ProcessTargetSpeed(const GeneralMessage &msg, const NodeInfo &src);

  public:
    TrainApp() = delete;

    TrainApp(int rid, int nid)
        : GeneralApp(rid, nid),
          cur_enb(0)
    {
      this->type = TRAIN;
    }

    int cur_enb; //!< For handover
    /**
     * @note This is the ground truth, only for the use of simulator printing log.
     * The controller should rely on the sensor data instead.
     */
    double get_position();
    double get_speed();
    double get_acc();

    virtual void ProcessLanMsg(Ptr<Socket> socket);
    virtual void ProcessWirelessMsg(Ptr<Socket> socket);
    void setMobilityInfo(double speed, double acc, double pos);
  };
} // namespace ns3

#endif