/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "dram_controller.h"

#include <algorithm>

#include "champsim_constants.h"
#include "util.h"

extern uint8_t all_warmup_complete;

struct is_unscheduled {
  bool operator()(const PACKET& lhs) { return !lhs.scheduled; }
};

struct next_schedule : public invalid_is_maximal<PACKET, min_event_cycle<PACKET>, PACKET, is_unscheduled, is_unscheduled> {
};

void MEMORY_CONTROLLER::operate()
{
  for (auto& channel : channels) {
    while(1){
      auto minIter = channel.RQ.end();
      for (auto iter=channel.RQ.begin();iter!=channel.RQ.end();iter++){
        if(iter->valid && (iter->event_cycle <=current_cycle)){
          minIter=iter;
          break;
        }
      }
      if(minIter==channel.RQ.end()) break;
      for(auto ret : minIter->pkt.to_return){
        ret->return_data(&(minIter->pkt));
      }
      *minIter={};
    }
    //repeat for WQ?
    while(1){
      auto minIter = channel.WQ.end();
      for (auto iter=channel.WQ.begin();iter!=channel.WQ.end();iter++){
        if(iter->valid && (iter->event_cycle <=current_cycle)){
          minIter=iter;
          break;
        }
      }
      if(minIter==channel.WQ.end()) break;
      for(auto ret : minIter->pkt.to_return){
        ret->return_data(&(minIter->pkt));
      }
      *minIter={};
    }
  }

}

int MEMORY_CONTROLLER::add_rq(PACKET* packet)
{
  //std::cout<<"code gets here?"<<std::endl;
  if (all_warmup_complete < NUM_CPUS) {
    for (auto ret : packet->to_return)
      ret->return_data(packet);

    return -1; // Fast-forward
  }

  
  //keeping channels. Could use separate channels to distinguish local/1hop/2hop/CI
  auto& channel = channels[dram_get_channel(packet->address)]; 

  // Check for forwarding
  auto wq_it = std::find_if(std::begin(channel.WQ), std::end(channel.WQ), eq_addr<M_REQUEST>(packet->address, LOG2_BLOCK_SIZE));
  if (wq_it != std::end(channel.WQ)) {
    packet->data = wq_it->pkt.data;
    for (auto ret : packet->to_return)
      ret->return_data(packet);

    return -1; // merged index
  }

  // Check for duplicates
  auto rq_it = std::find_if(std::begin(channel.RQ), std::end(channel.RQ), eq_addr<M_REQUEST>(packet->address, LOG2_BLOCK_SIZE));
  if (rq_it != std::end(channel.RQ)) {
    packet_dep_merge(rq_it->pkt.lq_index_depend_on_me, packet->lq_index_depend_on_me);
    packet_dep_merge(rq_it->pkt.sq_index_depend_on_me, packet->sq_index_depend_on_me);
    packet_dep_merge(rq_it->pkt.instr_depend_on_me, packet->instr_depend_on_me);
    packet_dep_merge(rq_it->pkt.to_return, packet->to_return);

    return std::distance(std::begin(channel.RQ), rq_it); // merged index
  }

  // Find empty slot
  rq_it = std::find_if_not(std::begin(channel.RQ), std::end(channel.RQ), is_valid<M_REQUEST>());
  if (rq_it == std::end(channel.RQ)) {
    return 0;
  }
  PACKET tpkt=*packet;
  M_REQUEST mrq = {true, (current_cycle+channel.ch_latency),packet->address, tpkt};
  *rq_it = mrq;
  //rq_it->event_cycle = current_cycle;
  //std::cout<<"RQ entry added"<<std::endl;
  return get_occupancy(1, packet->address);
}

int MEMORY_CONTROLLER::add_wq(PACKET* packet)
{
  //return add_rq(packet);
  
  if (all_warmup_complete < NUM_CPUS)
    return -1; // Fast-forward

  auto& channel = channels[dram_get_channel(packet->address)];

  // Check for duplicates
  auto wq_it = std::find_if(std::begin(channel.WQ), std::end(channel.WQ), eq_addr<M_REQUEST>(packet->address, LOG2_BLOCK_SIZE));
  if (wq_it != std::end(channel.WQ))
    return 0;

  // search for the empty index
  wq_it = std::find_if_not(std::begin(channel.WQ), std::end(channel.WQ), is_valid<M_REQUEST>());
  if (wq_it == std::end(channel.WQ)) {
    return -2;
  }
  PACKET tpkt=*packet;
  M_REQUEST mrq = {true, (current_cycle+channel.ch_latency),packet->address, tpkt};
  *wq_it = mrq;
  //wq_it->event_cycle = current_cycle;

  return get_occupancy(2, packet->address);
}

int MEMORY_CONTROLLER::add_pq(PACKET* packet) { return add_rq(packet); }


uint32_t MEMORY_CONTROLLER::dram_get_channel(uint64_t address)
{
  // int rint=rand() % 295;
  // int chan=0;
  // if(rint<46) chan=0;
  // else if(rint<95) chan=1;
  // else chan=2;

  int rint=rand() % 295;
  int chan=0;
  if(rint<33) chan=0;
  else if(rint<43) chan=1;
  else if(rint<88) chan=2;
  else chan=3;
  return chan;
  //int shift = LOG2_BLOCK_SIZE;
  //return (address >> shift) & bitmask(lg2(DRAM_CHANNELS));
}

uint32_t MEMORY_CONTROLLER::get_occupancy(uint8_t queue_type, uint64_t address)
{
  uint32_t channel = dram_get_channel(address);
  //std::cout<<"channel: "<<channel<<std::endl;
  if (queue_type == 1){
    //std::cout<<"get occupancy: "<<std::count_if(std::begin(channels[channel].RQ), std::end(channels[channel].RQ), is_valid<M_REQUEST>())<<std::endl;
    return std::count_if(std::begin(channels[channel].RQ), std::end(channels[channel].RQ), is_valid<M_REQUEST>());
  }
  else if (queue_type == 2)
    return std::count_if(std::begin(channels[channel].WQ), std::end(channels[channel].WQ), is_valid<M_REQUEST>());
  else if (queue_type == 3)
    return get_occupancy(1, address);

  return 0;
}

uint32_t MEMORY_CONTROLLER::get_size(uint8_t queue_type, uint64_t address)
{
  uint32_t channel = dram_get_channel(address);
  if (queue_type == 1){
    //std::cout<<"RQsize at get_size: "<<channels[channel].RQ.size()<<std::endl;    
    return channels[channel].RQ.size();
  }
  else if (queue_type == 2)
    return channels[channel].WQ.size();
  else if (queue_type == 3)
    return get_size(1, address);

  return 0;
}
