#this was initiated by atom(conan)
#partially modified by opkr
import os
import math
from cereal import car, log
from common.params import Params

from selfdrive.car.hyundai.spdcontroller  import SpdController

import common.log as trace1

LaneChangeState = log.LateralPlan.LaneChangeState


class SpdctrlRelaxed(SpdController):
    def __init__(self, CP=None):
        super().__init__( CP )
        self.cv_Raio = 0.45
        self.cv_Dist = -5
        self.steer_mode = ""
        self.cruise_gap = 0.0
        self.cut_in = False
        self.map_enable = False
        self.map_spdlimit_offset = 0
        self.target_speed = 0
        self.target_speed_camera = 0
        self.target_speed_map = 0.0
        self.target_speed_map_counter = 0
        self.target_speed_map_counter1 = 0
        self.target_speed_map_counter2 = 0
        self.hesitant_status = False
        self.hesitant_timer = 0
        self.map_decel_only = False
        self.map_spdlimit_offset = int(Params().get("OpkrSpeedLimitOffset", encoding="utf8"))
        self.map_enabled = False
        self.second = 0

    def update_lead(self, sm, CS, dRel, yRel, vRel, CC):

        self.map_decel_only = CS.out.cruiseState.modeSel == 5
        plan = sm['longitudinalPlan']
        dRele = plan.dRel1 #EON Lead
        yRele = plan.yRel1 #EON Lead
        vRele = plan.vRel1 * 3.6 + 0.5 #EON Lead
        dRelef = plan.dRel2 #EON Lead
        yRelef = plan.yRel2 #EON Lead
        vRelef = plan.vRel2 * 3.6 + 0.5 #EON Lead
        lead2_status = plan.status2
        self.second += 1
        if self.second > 100:
            self.map_enabled = Params().get_bool("OpkrMapEnable")
            self.second = 0
        if self.map_enabled:
            self.target_speed_camera = plan.targetSpeedCamera + round(plan.targetSpeedCamera*0.01*self.map_spdlimit_offset)
        else:
            self.target_speed_camera = CS.out.safetySign + round(CS.out.safetySign*0.01*self.map_spdlimit_offset)
        
        if self.target_speed_camera <= 29:
            self.map_enable = False
            self.target_speed = 0
        elif self.target_speed_camera > 29 and (plan.onSpeedControl if self.map_enabled else CS.on_speed_control):
            self.target_speed = self.target_speed_camera
            self.map_enable = True
        else:
            self.target_speed = 0
            self.map_enable = False

        lead_set_speed = int(round(self.cruise_set_speed_kph))
        lead_wait_cmd = 250

        dRel = 150
        vRel = 0
        dRel2 = 140
        vRel2 = 0

        #dRel, yRel, vRel = self.get_lead( sm, CS )
        if 1 < dRele < 149:
            dRel = int(dRele) # dRele(?????? ????????????)??? ??????
            vRel = int(vRele)
        elif 1 < CS.lead_distance < 149:
            dRel = int(CS.lead_distance) # CS.lead_distance(????????? ????????????)??? ??????
            vRel = int(CS.lead_objspd)
        else:
            dRel = 150
            vRel = 0

        if 1 < dRelef < 140:
            dRel2 = int(dRelef)
            vRel2 = int(vRelef) # for cut-in detection??

        dst_lead_distance = int(CS.clu_Vanz*self.cv_Raio)   # ?????? ?????? ??????
        dst_lead_distance2 = int(CS.clu_Vanz*0.45)   # ?????? ?????? ??????
        
        if dst_lead_distance > 100:
            dst_lead_distance = 100
        #elif dst_lead_distance < 15:
            #dst_lead_distance = 15

        if 1 < dRel < 149: #???????????? ????????? 150?????? ????????????, ??? ????????? ????????????,
            self.time_no_lean = 0
            d_delta = dRel - dst_lead_distance  # d_delta = ????????????(?????????) - ????????????
            lead_objspd = vRel  # ???????????? ????????????.
        else:
            d_delta = 0
            lead_objspd = 0

        if 1 < dRel2 < 140:
            d_delta2 = dRel2 - dst_lead_distance2
        else:
            d_delta2 = 0
 
        if CS.driverAcc_time and not self.map_decel_only: #???????????? ???????????? ????????? ????????? ??????????????? ????????????+1??? ?????????
            if int(CS.VSetDis) < int(round(CS.clu_Vanz)) + 1:
              lead_set_speed = int(round(CS.clu_Vanz)) + 1
              self.seq_step_debug = "???????????????"
              lead_wait_cmd = 8
        elif int(round(self.target_speed)) < int(CS.VSetDis) and self.map_enable and ((int(round(self.target_speed)) < int(round(self.cruise_set_speed_kph))) and self.target_speed != 0):
            self.seq_step_debug = "???????????????"
            lead_wait_cmd, lead_set_speed = self.get_tm_speed(CS, 8, -1)
        elif CC.res_speed != 0 and CC.res_speed < int(CS.VSetDis):
            self.seq_step_debug = "RES????????????"
            lead_wait_cmd, lead_set_speed = self.get_tm_speed(CS, 8, -1)
        # ?????? ?????? ??????
        elif CS.out.cruiseState.modeSel in [1,2,4] and d_delta < 0 or d_delta2 < 0 and not self.map_decel_only: # ??????????????????(????????????*0.4)?????? ????????? ?????? ??? ??????
            if (int(CS.clu_Vanz)-1) <= int(CS.VSetDis) and dRele - dRelef > 3 and lead2_status:
                self.seq_step_debug = "??????????????????"
                #lead_wait_cmd, lead_set_speed = self.get_tm_speed(CS, 15, -5)
                self.cut_in = True
            elif lead_objspd < 0 and self.cut_in == True and (int(CS.clu_Vanz)-7) <= int(CS.VSetDis) and dRele < int(CS.clu_Vanz)*0.3 and int(CS.clu_Vanz) > 80:
                self.seq_step_debug = "????????????3"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed(CS, 8, -1)
            elif lead_objspd < 0 and self.cut_in == True and (int(CS.clu_Vanz)-5) <= int(CS.VSetDis) and dRele < int(CS.clu_Vanz)*0.35 and int(CS.clu_Vanz) > 50:
                self.seq_step_debug = "????????????2"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed(CS, 8, -1)
            elif lead_objspd < 0 and self.cut_in == True and (int(CS.clu_Vanz)-3) <= int(CS.VSetDis) and dRele < int(CS.clu_Vanz)*0.4 and int(CS.clu_Vanz) > 20:
                self.seq_step_debug = "????????????1"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed(CS, 8, -1)
            elif lead_objspd <= 0 and self.cut_in == True and (int(CS.clu_Vanz)-4) <= int(CS.VSetDis):
                self.seq_step_debug = "?????????????????????"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed(CS, 8, -1)
            elif lead_objspd < -30 or (dRel < 60 and CS.clu_Vanz > 60 and lead_objspd < -5) and (int(CS.clu_Vanz)-6) <= int(CS.VSetDis): # ????????? ?????? ????????? ?????? ??????
                self.seq_step_debug = "?????????,-5"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed(CS, 8, -5)
                self.cut_in = False
            elif lead_objspd < -20 or (dRel < 80 and CS.clu_Vanz > 80 and lead_objspd < -5) and (int(CS.clu_Vanz)-5) <= int(CS.VSetDis):  # ????????? ?????? ????????? ?????? ??????
                self.seq_step_debug = "?????????,-4"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed(CS, 8, -4)
                self.cut_in = False
            elif lead_objspd < 0 and int(CS.clu_Vanz)//abs(lead_objspd) <= int(CS.VSetDis)//abs(lead_objspd):
                self.seq_step_debug = "?????????-??????"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed(CS, max(8, 120-(abs(lead_objspd**3))), -2)
                self.cut_in = False
            elif lead_objspd > 3 and int(CS.clu_Vanz) <= int(CS.VSetDis): 
                self.seq_step_debug = "?????????,????????????"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed(CS, 60, 1)
                self.cut_in = False
            elif lead_objspd >= 0 and int(CS.clu_Vanz) <= int(CS.VSetDis): 
                self.seq_step_debug = "?????????,-1"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed(CS, 240, -1)
                self.cut_in = False
            else:
                self.seq_step_debug = "????????????"
                self.cut_in = False
        # ??????????????? ?????? ?????? ???????????? ?????? ??????
        elif CS.out.cruiseState.modeSel in [1,2,4] and 20 <= dRel < 149 and lead_objspd < -15 and not self.map_decel_only: #?????? ?????? ??? ????????? ?????? ?????? ???
            self.cut_in = False
            if int(CS.clu_Vanz//abs(lead_objspd)) <= int(CS.VSetDis//abs(lead_objspd)):
              self.seq_step_debug = "???????????? ??????"
              lead_wait_cmd, lead_set_speed = self.get_tm_speed(CS, 8, -20)
            # if dRel >= 50:
            #     self.seq_step_debug = "???????????? ??????-20"
            #     lead_wait_cmd, lead_set_speed = self.get_tm_speed(CS,  10, -20)
            # elif dRel >= 40:
            #     self.seq_step_debug = "???????????? ??????-15"
            #     lead_wait_cmd, lead_set_speed = self.get_tm_speed(CS, 10, -15)
            # elif dRel >= 30:
            #     self.seq_step_debug = "???????????? ??????-10"
            #     lead_wait_cmd, lead_set_speed = self.get_tm_speed(CS, 10, -10)
        elif self.cruise_set_speed_kph > int(round((CS.clu_Vanz))) and not self.map_decel_only:  #????????????????????? ?????????????????? ?????????
            self.cut_in = False
            if 10 > dRel > 3 and lead_objspd <= 0 and 1 < int(CS.clu_Vanz) <= 7 and CS.VSetDis < 45 and ((int(round(self.target_speed)) > int(CS.VSetDis) and self.target_speed != 0) or self.target_speed == 0):
                self.seq_step_debug = "??????????????????"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed( CS, 8, 5)
            elif 20 > dRel > 3 and lead_objspd > 5 and CS.clu_Vanz <= 25 and CS.VSetDis < 55 and ((int(round(self.target_speed)) > int(CS.VSetDis) and self.target_speed != 0) or self.target_speed == 0):
                self.seq_step_debug = "SS>VS,??????"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed( CS, 100, 1)
            #elif lead_objspd > 9 and CS.clu_Vanz > 20 and CS.VSetDis < 45: # ??????????????? ???????????? ???????????? ??? ???????????? ?????? ???
            #    self.seq_step_debug = "SS>VS,??????"
            #    lead_wait_cmd, lead_set_speed = self.get_tm_speed( CS, 10, 5)
            #elif lead_objspd > 8 and CS.clu_Vanz > 45 and CS.VSetDis < 60: # ?????????????????? ???????????? ???????????? ??? ???????????? ?????? ???
            #    self.seq_step_debug = "SS>VS,??????"
            #    lead_wait_cmd, lead_set_speed = self.get_tm_speed( CS, 15, 5)
            #elif lead_objspd > 7 and CS.clu_Vanz > 65 and CS.VSetDis < 80:
            #    self.seq_step_debug = "SS>VS,??????"
            #    lead_wait_cmd, lead_set_speed = self.get_tm_speed( CS, 15, 5)
            elif lead_objspd > 0 and int(CS.clu_Vanz//lead_objspd) >= int(CS.VSetDis//lead_objspd) and int(CS.clu_Vanz*0.4) < dRel < 149 and ((int(round(self.target_speed)) > int(CS.VSetDis) and self.target_speed != 0) or self.target_speed == 0):
                self.seq_step_debug = "SS>VS,++1"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed( CS, 8, 1)
            elif lead_objspd > 0 and int(CS.clu_Vanz)+lead_objspd >= int(CS.VSetDis) and int(CS.clu_Vanz*0.4) < dRel < 149 and ((int(round(self.target_speed)) > int(CS.VSetDis) and self.target_speed != 0) or self.target_speed == 0):
                self.seq_step_debug = "SS>VS,+1"
                if int(CS.VSetDis) > int(CS.clu_Vanz)+14:
                    self.hesitant_status = True
                lead_wait_cmd, lead_set_speed = self.get_tm_speed( CS, 8, 1)
            elif CS.clu_Vanz > 80 and lead_objspd < -1 and (int(CS.clu_Vanz)-1) <= int(CS.VSetDis) and int(CS.clu_Vanz) >= dRel*1.6 and 1 < dRel < 149: # ???????????? ?????? ??? ?????? ?????? ?????? ????????? ????????????/2 ????????? ?????? ???????????? ??? ??????????????? ?????? ????????? ??????
                self.seq_step_debug = "SS>VS,v>80,-1"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed( CS, max(8, 50+(lead_objspd*2)), -1)
            elif CS.clu_Vanz > 60 and lead_objspd < -2 and (int(CS.clu_Vanz)-1) <= int(CS.VSetDis) and int(CS.clu_Vanz) >= dRel*1.8 and 1 < dRel < 149: # ???????????? ?????? ??? ?????? ?????? ?????? ????????? ????????????/2 ????????? ?????? ???????????? ??? ??????????????? ?????? ????????? ??????
                self.seq_step_debug = "SS>VS,v>60,-1"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed( CS, max(8, 50+(lead_objspd*2)), -1)
            elif CS.clu_Vanz > 40 and lead_objspd < -2 and (int(CS.clu_Vanz)-1) <= int(CS.VSetDis) and int(CS.clu_Vanz) >= dRel*2.1 and 1 < dRel < 149: # ???????????? ?????? ??? ?????? ?????? ?????? ????????? ????????????/2 ????????? ?????? ???????????? ??? ??????????????? ?????? ????????? ??????
                self.seq_step_debug = "SS>VS,v>40,-1"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed( CS, max(8, 50+(lead_objspd*2)), -1)
            elif 70 > CS.clu_Vanz > 30 and lead_objspd < -2 and int(CS.clu_Vanz)//abs(lead_objspd*2.2) <= int(CS.VSetDis)//abs(lead_objspd*2.2) and int(CS.clu_Vanz) >= dRel*0.8 and 1 < dRel < 149:
                self.seq_step_debug = "SS>VS,70>v>30,-1"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed( CS, max(8, 120-(abs(lead_objspd**3))), -2)
            elif 7 < int(CS.clu_Vanz) < 30 and lead_objspd < 0 and CS.VSetDis > 30:
                self.seq_step_debug = "SS>VS,30??????"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed( CS, 8, -5)
            elif lead_objspd <= 0 and int(CS.clu_Vanz)+4 <= int(CS.VSetDis) and int(CS.clu_Vanz) > 40 and 1 < dRel < 149: # ????????? ?????? ?????? ??? ????????????+5??? ????????????????????? ??????
                self.seq_step_debug = "SS>VS,vRel<=0"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed( CS, 8, -1)
            elif d_delta == 0 and lead_objspd == 0 and int(CS.clu_Vanz//10) >= int(CS.VSetDis//10) and dRel > 149 and ((int(round(self.target_speed)) > int(CS.VSetDis) and self.target_speed != 0) or self.target_speed == 0):
                self.seq_step_debug = "???????????????"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed( CS, 8, 5)
            elif d_delta == 0 and lead_objspd == 0 and self.cruise_set_speed_kph > int(CS.VSetDis) and int(CS.clu_Vanz//10) >= int(CS.VSetDis//10) and dRel > 149 and ((int(round(self.target_speed)) > int(CS.VSetDis) and self.target_speed != 0) or self.target_speed == 0):
                self.seq_step_debug = "????????????"
                lead_wait_cmd, lead_set_speed = self.get_tm_speed( CS, 8, 1)
            elif lead_objspd == 0 and int(CS.clu_Vanz) == 0 and dRel <= 6:
                self.seq_step_debug = "????????????"
            else:
                self.seq_step_debug = "SS>VS,????????????"
            if self.hesitant_status and self.hesitant_timer > 150:
                self.hesitant_status = False
                self.hesitant_timer = 0
            elif self.hesitant_status:
                self.hesitant_timer += 1
        # ???????????? ?????? ??? ?????? ??????
        elif CS.out.cruiseState.modeSel in [1,2,4] and 20 <= dRel < int(CS.clu_Vanz*0.75) and lead_objspd < -1 and not self.map_decel_only:
            self.cut_in = False
            if int(CS.clu_Vanz//abs(lead_objspd)) <= int(CS.VSetDis//abs(lead_objspd)):
              self.seq_step_debug = "????????????"
              lead_wait_cmd, lead_set_speed = self.get_tm_speed(CS, max(8, 200-(abs(lead_objspd**3))), -1)
        elif CS.out.cruiseState.modeSel in [1,2,4] and lead_objspd >= 0 and CS.clu_Vanz >= int(CS.VSetDis) and int(CS.clu_Vanz * 0.5) < dRel < 149 and not self.map_decel_only:
            self.cut_in = False
            self.seq_step_debug = "????????????"
        elif (self.map_decel_only or CS.out.cruiseState.modeSel == 3) and self.cruise_set_speed_kph > int(round(CS.VSetDis)) and ((int(round(self.target_speed)) > int(CS.VSetDis) and self.target_speed != 0) or self.target_speed == 0):
            self.seq_step_debug = "????????????"
            lead_wait_cmd, lead_set_speed = self.get_tm_speed( CS, 8, 1)
        else:
            self.cut_in = False
            self.seq_step_debug = "????????????"

        return lead_wait_cmd, lead_set_speed

    def update_curv(self, CS, sm, curve_speed):
        wait_time_cmd = 0
        set_speed = self.cruise_set_speed_kph

        # 2. ?????? ??????.
        #if self.cruise_set_speed_kph >= 100:
        if CS.out.cruiseState.modeSel in [1,3,4] and sm['lateralPlan'].laneChangeState == LaneChangeState.off and not (CS.out.leftBlinker or CS.out.rightBlinker)and not self.map_decel_only:
            cam_speed = self.target_speed if self.target_speed > 0 else 255
            if curve_speed < 35 and int(CS.clu_Vanz) >= 40 and CS.lead_distance >= 15:
                set_speed = min(cam_speed, 45, self.cruise_set_speed_kph, self.cruise_set_speed_kph - int(CS.clu_Vanz * 0.3))
                self.seq_step_debug = "????????????-5"
                wait_time_cmd = 15
            elif curve_speed < 40 and int(CS.clu_Vanz) >= 40 and CS.lead_distance >= 15:
                set_speed = min(cam_speed, 55, self.cruise_set_speed_kph, self.cruise_set_speed_kph - int(CS.clu_Vanz * 0.25))
                self.seq_step_debug = "????????????-4"
                wait_time_cmd = 30
            elif curve_speed < 60 and int(CS.clu_Vanz) >= 40 and CS.lead_distance >= 15:
                set_speed = min(cam_speed, 65, self.cruise_set_speed_kph, self.cruise_set_speed_kph - int(CS.clu_Vanz * 0.2))
                self.seq_step_debug = "????????????-3"
                wait_time_cmd = 45
            elif curve_speed < 75 and int(CS.clu_Vanz) >= 40 and CS.lead_distance >= 15:
                set_speed = min(cam_speed, 75, self.cruise_set_speed_kph, self.cruise_set_speed_kph - int(CS.clu_Vanz * 0.15))
                self.seq_step_debug = "????????????-2"
                wait_time_cmd = 60
            elif curve_speed < 90 and int(CS.clu_Vanz) >= 40 and CS.lead_distance >= 15:
                set_speed = min(cam_speed, 85, self.cruise_set_speed_kph, self.cruise_set_speed_kph - int(CS.clu_Vanz * 0.1))
                self.seq_step_debug = "????????????-1"
                wait_time_cmd = 75

        return wait_time_cmd, set_speed


    def update_log(self, CS, set_speed, target_set_speed, long_wait_cmd ):
        if CS.out.cruiseState.modeSel == 0:
            self.steer_mode = "????????????"
        elif CS.out.cruiseState.modeSel == 1:
            self.steer_mode = "??????+??????"
        elif CS.out.cruiseState.modeSel == 2:
            self.steer_mode = "??????ONLY"
        elif CS.out.cruiseState.modeSel == 3:
            self.steer_mode = "??????ONLY"
        elif CS.out.cruiseState.modeSel == 4:
            self.steer_mode = "??????1??????"
        elif CS.out.cruiseState.modeSel == 5:
            self.steer_mode = "?????????ONLY"

        if self.cruise_gap != CS.cruiseGapSet:
            self.cruise_gap = CS.cruiseGapSet

        str3 = 'MODE={:s}  VL={:03.0f}/{:03.0f}  TM={:03.0f}/{:03.0f}  TS={:03.0f}'.format( self.steer_mode, set_speed, CS.VSetDis, long_wait_cmd, self.long_curv_timer, int(round(self.target_speed)) )
        str4 = '  RD=D:{:03.0f}/V:{:03.0f}  CG={:1.0f}  DG={:s}'.format(  CS.lead_distance, CS.lead_objspd, self.cruise_gap, self.seq_step_debug )

        str5 = str3 + str4
        trace1.printf2( str5 )
