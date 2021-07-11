#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include "helper.h"
#include "writeOutput.h"



bool is_array_contain_index(int* array,int size,int index) {
    for (int i=0;i<size;i++) {
        if (array[i] == index) return true;
    }
    return false;
}

int* get_min_elements_index_array(int* array,int size) {
    int* return_array = (int *) malloc(sizeof(int)*size);
    for (int i=0;i<size;i++) return_array[i] = -1; 
    for (int i=0;i<size;i++) {
        int min = INT_MAX;
        int min_index = -1;
        for (int j=0;j<size;j++) {
            if (array[j]<min && !(is_array_contain_index(return_array,size,j))) {
                min = array[j];
                min_index = j;
            }
        }
        return_array[i] = min_index;
    }
    return return_array;
}


typedef struct {
    sem_t lock_outgoing_space_info;
    bool is_empty;
    bool is_drone_take_this_space;
    PackageInfo* package;
} outgoing_space;


typedef struct {
    sem_t lock_incoming_space_info;    
    bool is_empty;
    bool is_drone_arrived;
    PackageInfo* package;
} incoming_space;


typedef struct {
    sem_t lock_charging_space_info;
    bool is_empty;
    int drone_id;
} charging_space;


typedef struct {
    sem_t lock_drone_info;
    bool is_active;
    bool transport_package;
    PackageInfo* package;
    bool nearby_hub_request;
    int nearby_hub_id;
    bool drone_come_to_nearby_hub;
    int current_range;
    int drone_id;
    int release_outgoing_space_index;
} drone;


typedef struct {
    sem_t lock_hub_info;
    bool is_active;
    int hub_id;
    int init_outgoing_space_length;
    int init_incoming_space_length;
    int init_charging_space_length;
    outgoing_space* hub_outgoing_space;
    incoming_space* hub_incoming_space;
    charging_space* hub_charging_space; 
    int* distance_to_other_hubs; 
} hub;


int number_of_hubs_at_the_beginning;
int number_of_drones_at_the_beginning;
int current_number_of_hubs;
int current_number_of_drones;
int current_number_of_senders;
int current_number_of_receivers;
int barrier_for_sender;
int number_of_sended_packages;
sem_t lock_current_number_of_senders;
sem_t lock_current_number_of_hubs;
sem_t lock_current_number_of_drones;
sem_t lock_current_number_of_receivers;
sem_t lock_barrier_for_sender;
sem_t lock_number_of_sended_packages;
hub* hub_array;
drone* drone_array;
int* receiver_hubs;
int* sender_hubs;




void sender_thread(void* arguments) {  
    int* arguments_as_int = (int *) arguments;
    int sender_id,sender_waiting_time,remaining_packages,assigned_hub_id;
    SenderInfo sender_info;
    PackageInfo package_info;
    sender_id = arguments_as_int[0];
    sender_waiting_time = arguments_as_int[1];
    remaining_packages = arguments_as_int[2];
    assigned_hub_id = arguments_as_int[3];
    free(arguments);    
    long long sleep_duration = sender_waiting_time*UNIT_TIME;
    FillSenderInfo(&sender_info,sender_id,assigned_hub_id,remaining_packages,NULL);
    WriteOutput(&sender_info,NULL,NULL,NULL,SENDER_CREATED);
    srand(time(NULL));
    int random_receiver,random_hub;
    bool flag_for_break_the_loop; 
    
    // Waiting for all hubs to be created.
    while (true) {
        sem_wait(&(lock_barrier_for_sender));
        if (barrier_for_sender == number_of_hubs_at_the_beginning) {
            sem_post(&(lock_barrier_for_sender));
            break;
        }
        sem_post(&(lock_barrier_for_sender));
    }

    // Waiting for all receivers to be created.
    while (true) {
        sem_wait(&(lock_current_number_of_receivers));
        if (current_number_of_receivers == number_of_hubs_at_the_beginning) {
            sem_post(&(lock_current_number_of_receivers));
            break;
        }
        sem_post(&(lock_current_number_of_receivers));
    }

    while (remaining_packages > 0) {
        flag_for_break_the_loop = true;
        while (true) {
            random_receiver = (rand()%number_of_hubs_at_the_beginning) + 1;
            random_hub = receiver_hubs[random_receiver-1];
            if (random_hub != assigned_hub_id) break;
        }
        // We decided random receiver and get its hub id from global array.    
        // Then we will wait for available outgoing space.
        while (flag_for_break_the_loop) {
            for (int i=0; i < hub_array[assigned_hub_id-1].init_outgoing_space_length;i++) {
                sem_wait(&(hub_array[assigned_hub_id-1].hub_outgoing_space[i].lock_outgoing_space_info));
                if (hub_array[assigned_hub_id-1].hub_outgoing_space[i].is_empty) {
                    hub_array[assigned_hub_id-1].hub_outgoing_space[i].is_empty = false;
                    hub_array[assigned_hub_id-1].hub_outgoing_space[i].is_drone_take_this_space = false;
                    FillPacketInfo(hub_array[assigned_hub_id-1].hub_outgoing_space[i].package,sender_id,assigned_hub_id,random_receiver,random_hub);
                    flag_for_break_the_loop = false;
                    sem_post(&(hub_array[assigned_hub_id-1].hub_outgoing_space[i].lock_outgoing_space_info));   
                    break;
                }
                sem_post(&(hub_array[assigned_hub_id-1].hub_outgoing_space[i].lock_outgoing_space_info));   
            }
        }
        FillPacketInfo(&package_info,sender_id,assigned_hub_id,random_receiver,random_hub);
        FillSenderInfo(&sender_info,sender_id,assigned_hub_id,remaining_packages,&package_info);
        WriteOutput(&sender_info,NULL,NULL,NULL,SENDER_DEPOSITED);
        remaining_packages = remaining_packages-1;
        wait(sleep_duration);
    }
    sem_wait(&(lock_current_number_of_senders));
    current_number_of_senders = current_number_of_senders - 1;
    FillSenderInfo(&sender_info,sender_id,assigned_hub_id,remaining_packages,NULL); 
    WriteOutput(&sender_info,NULL,NULL,NULL,SENDER_STOPPED);
    sem_post(&(lock_current_number_of_senders));
    return;
}


void receiver_thread(void* arguments) { 
    int* arguments_as_int = (int *) arguments;
    int receiver_id,receiver_waiting_time,assigned_hub_id;
    receiver_id = arguments_as_int[0];
    receiver_waiting_time = arguments_as_int[1];
    assigned_hub_id = arguments_as_int[2];
    free(arguments);
    long long sleep_duration = receiver_waiting_time*UNIT_TIME;
    ReceiverInfo receiver_info;
    PackageInfo package_info;
    FillReceiverInfo(&receiver_info,receiver_id,assigned_hub_id,NULL);
    WriteOutput(NULL,&receiver_info,NULL,NULL,RECEIVER_CREATED);
    
    sem_wait(&(lock_current_number_of_receivers));
    current_number_of_receivers = current_number_of_receivers + 1;
    sem_post(&(lock_current_number_of_receivers));

    while (true) {
        sem_wait(&(lock_current_number_of_hubs));
        if (current_number_of_hubs <= 0) {
            sem_post(&(lock_current_number_of_hubs));
            break;
        }
        sem_post(&(lock_current_number_of_hubs));
        for (int i=0;i<hub_array[assigned_hub_id-1].init_incoming_space_length;i++) {
            sem_wait(&(hub_array[assigned_hub_id-1].hub_incoming_space[i].lock_incoming_space_info));
            // Incoming alanı doluysa ve drone da gelmişse (yolda değilse yani) paketi alıp alanı boşaltıyoruz.
            if (!(hub_array[assigned_hub_id-1].hub_incoming_space[i].is_empty) && hub_array[assigned_hub_id-1].hub_incoming_space[i].is_drone_arrived) {
                hub_array[assigned_hub_id-1].hub_incoming_space[i].is_empty = true;  
                hub_array[assigned_hub_id-1].hub_incoming_space[i].is_drone_arrived = false;
                int sender_ID = hub_array[assigned_hub_id-1].hub_incoming_space[i].package->sender_id;
                int sending_hub_ID = hub_array[assigned_hub_id-1].hub_incoming_space[i].package->sending_hub_id;
                sem_post(&(hub_array[assigned_hub_id-1].hub_incoming_space[i].lock_incoming_space_info));
                FillPacketInfo(&package_info,sender_ID,sending_hub_ID,receiver_id,assigned_hub_id);
                FillReceiverInfo(&receiver_info,receiver_id,assigned_hub_id,&package_info);
                WriteOutput(NULL,&receiver_info,NULL,NULL,RECEIVER_PICKUP);
                sem_wait(&(lock_number_of_sended_packages));
                number_of_sended_packages = number_of_sended_packages - 1;
                sem_post(&(lock_number_of_sended_packages));
                wait(sleep_duration);
            }
            else {
                sem_post(&(hub_array[assigned_hub_id-1].hub_incoming_space[i].lock_incoming_space_info));
            }
        }
    }
    FillReceiverInfo(&receiver_info,receiver_id,assigned_hub_id,NULL);
    WriteOutput(NULL,&receiver_info,NULL,NULL,RECEIVER_STOPPED);
    return;
}


void drone_thread(void* arguments) { 
    int* arguments_as_int = (int *) arguments;
    int drone_id,drone_speed,current_hub_id,current_range,max_range;
    drone_id = arguments_as_int[0];
    drone_speed = arguments_as_int[1];
    current_hub_id = arguments_as_int[2];
    current_range = arguments_as_int[3];
    max_range = current_range;
    DroneInfo drone_info;
    PackageInfo package_info;
    long long waiting_time;
    long long drone_parked_time = timeInMilliseconds();
    free(arguments);
    int error_no;
    bool flag_for_break_the_loop = false;
    FillDroneInfo(&drone_info,drone_id,current_hub_id,current_range,NULL,0);
    WriteOutput(NULL,NULL,&drone_info,NULL,DRONE_CREATED);
    
    // Assign itself to the inital hub charging space.
    for (int i=0;i<hub_array[current_hub_id-1].init_charging_space_length;i++) {
        sem_wait(&(hub_array[current_hub_id-1].hub_charging_space[i].lock_charging_space_info));
        if (hub_array[current_hub_id-1].hub_charging_space[i].is_empty) {
            hub_array[current_hub_id-1].hub_charging_space[i].is_empty = false;
            hub_array[current_hub_id-1].hub_charging_space[i].drone_id = drone_id;
            sem_post(&(hub_array[current_hub_id-1].hub_charging_space[i].lock_charging_space_info));
            break;
        }
        sem_post(&(hub_array[current_hub_id-1].hub_charging_space[i].lock_charging_space_info));
    }
    //
    sem_wait(&(lock_current_number_of_drones));
    current_number_of_drones = current_number_of_drones + 1;
    sem_post(&(lock_current_number_of_drones));


    while (true) {
        int i;
        bool transport_package_flag = false;
        bool nearby_hub_request_flag = false;
        while (true) {
            i++;
            if (i==1000000) {
                i=0;
                sem_wait(&(lock_current_number_of_hubs));
                if (current_number_of_hubs <= 0) {
                    sem_post(&(lock_current_number_of_hubs));
                    flag_for_break_the_loop = true;
                    break;
                }
                sem_post(&(lock_current_number_of_hubs));
            }
            sem_wait(&(drone_array[drone_id-1].lock_drone_info));
            if (drone_array[drone_id-1].is_active) {
                if (drone_array[drone_id-1].transport_package) {
                    transport_package_flag = true;
                }
                else if (drone_array[drone_id-1].nearby_hub_request) {
                    nearby_hub_request_flag = true;
                }
                sem_post(&(drone_array[drone_id-1].lock_drone_info));
                break;
            }
            sem_post(&(drone_array[drone_id-1].lock_drone_info));
        }
        if (flag_for_break_the_loop) break;
        if (transport_package_flag) {
            int distance = hub_array[current_hub_id-1].distance_to_other_hubs[(drone_array[drone_id-1].package->receiving_hub_id)-1]; 
            int destination_hub_id = drone_array[drone_id-1].package->receiving_hub_id;
            int incoming_space_index = -1;
            // Reserve incoming space at the destination hub.
            bool flag = true;
            while (flag) { 
                for (int i=0;i < hub_array[destination_hub_id-1].init_incoming_space_length;i++) {
                    sem_wait(&(hub_array[destination_hub_id-1].hub_incoming_space[i].lock_incoming_space_info));
                    if (hub_array[destination_hub_id-1].hub_incoming_space[i].is_empty) {
                        hub_array[destination_hub_id-1].hub_incoming_space[i].is_empty = false;
                        hub_array[destination_hub_id-1].hub_incoming_space[i].is_drone_arrived = false;
                        sem_wait(&(drone_array[drone_id-1].lock_drone_info));
                        hub_array[destination_hub_id-1].hub_incoming_space[i].package->receiver_id = drone_array[drone_id-1].package->receiver_id;
                        hub_array[destination_hub_id-1].hub_incoming_space[i].package->receiving_hub_id = drone_array[drone_id-1].package->receiving_hub_id;
                        hub_array[destination_hub_id-1].hub_incoming_space[i].package->sender_id = drone_array[drone_id-1].package->sender_id;
                        hub_array[destination_hub_id-1].hub_incoming_space[i].package->sending_hub_id = drone_array[drone_id-1].package->sending_hub_id;
                        FillPacketInfo(&package_info,drone_array[drone_id-1].package->sender_id,drone_array[drone_id-1].package->sending_hub_id,drone_array[drone_id-1].package->receiver_id,drone_array[drone_id-1].package->receiving_hub_id);
                        sem_post(&(drone_array[drone_id-1].lock_drone_info));
                        incoming_space_index = i;
                        flag = false;
                        sem_post(&(hub_array[destination_hub_id-1].hub_incoming_space[i].lock_incoming_space_info));
                        break;
                    }
                    sem_post(&(hub_array[destination_hub_id-1].hub_incoming_space[i].lock_incoming_space_info));
                }
            }

            flag = true;
            // Reserve charging space at the destination hub.
            while (flag) {
                for (int i=0;i < hub_array[destination_hub_id-1].init_charging_space_length;i++) {
                    sem_wait(&(hub_array[destination_hub_id-1].hub_charging_space[i].lock_charging_space_info));
                    if (hub_array[destination_hub_id-1].hub_charging_space[i].is_empty) {
                        hub_array[destination_hub_id-1].hub_charging_space[i].is_empty = false;
                        hub_array[destination_hub_id-1].hub_charging_space[i].drone_id = drone_id;
                        flag=false;
                        sem_post(&(hub_array[destination_hub_id-1].hub_charging_space[i].lock_charging_space_info));
                        break;
                    }
                    sem_post(&(hub_array[destination_hub_id-1].hub_charging_space[i].lock_charging_space_info));
                }
            }
            
            current_range = calculate_drone_charge(timeInMilliseconds()-drone_parked_time,current_range,max_range);

            // Current Range is low. Wait for charge.
            if ((distance/drone_speed) > current_range) {
                waiting_time = ((distance/drone_speed)-current_range) * UNIT_TIME;
                wait(waiting_time);
                current_range = calculate_drone_charge(waiting_time,current_range,max_range);
            }
            // Take package from outgoing storage. Release outgoing storage. Release charging space from current hub.
            
            // Hub bu bilgiyi eklemişti. Paketi götürmeye başlayınca bu indexi boşalt diye. Bu index bilgisini aldık.
            sem_wait(&(drone_array[drone_id-1].lock_drone_info));
            int release_outgoing_storage_index = drone_array[drone_id-1].release_outgoing_space_index;
            sem_post(&(drone_array[drone_id-1].lock_drone_info));
            //
            
            FillDroneInfo(&drone_info,drone_id,current_hub_id,current_range,&package_info,0);
            WriteOutput(NULL,NULL,&drone_info,NULL,DRONE_PICKUP);


            // Outgoing storage boşalttık current hubda. Sender boşalan yere yeni paketleri koyabilir. 
            sem_wait(&(hub_array[current_hub_id-1].hub_outgoing_space[release_outgoing_storage_index].lock_outgoing_space_info));
            hub_array[current_hub_id-1].hub_outgoing_space[release_outgoing_storage_index].is_empty = true;
            hub_array[current_hub_id-1].hub_outgoing_space[release_outgoing_storage_index].is_drone_take_this_space = false;
            sem_post(&(hub_array[current_hub_id-1].hub_outgoing_space[release_outgoing_storage_index].lock_outgoing_space_info));
            //
            
            

            // Charging space boşaltıyoruz current hubda. Charging space içinde gezip drone idlerine bakıyoruz. Bizimkiyle aynı olunca boşaltıyoruz.     
            for (int i=0;i<hub_array[current_hub_id-1].init_charging_space_length;i++) {
                sem_wait(&(hub_array[current_hub_id-1].hub_charging_space[i].lock_charging_space_info));
                if (hub_array[current_hub_id-1].hub_charging_space[i].drone_id == drone_id) {
                    hub_array[current_hub_id-1].hub_charging_space[i].is_empty = true;
                    sem_post(&(hub_array[current_hub_id-1].hub_charging_space[i].lock_charging_space_info));
                    break;
                }
                sem_post(&(hub_array[current_hub_id-1].hub_charging_space[i].lock_charging_space_info));
            }
            // 


            travel(distance,drone_speed);
            current_range = current_range - range_decrease(distance,drone_speed);
            current_hub_id = destination_hub_id;
            drone_parked_time = timeInMilliseconds();

            FillPacketInfo(&package_info,drone_array[drone_id-1].package->sender_id,drone_array[drone_id-1].package->sending_hub_id,drone_array[drone_id-1].package->receiver_id,drone_array[drone_id-1].package->receiving_hub_id);
            FillDroneInfo(&drone_info,drone_id,current_hub_id,current_range,&package_info,0);
            WriteOutput(NULL,NULL,&drone_info,NULL,DRONE_DEPOSITED);

            // Incoming spaceden yer ayırmıştık. Geldiğimizi haber veriyoruz drone arrived true yaparak.
            sem_wait(&(hub_array[destination_hub_id-1].hub_incoming_space[incoming_space_index].lock_incoming_space_info));
            hub_array[destination_hub_id-1].hub_incoming_space[incoming_space_index].is_drone_arrived = true;
            sem_post(&(hub_array[destination_hub_id-1].hub_incoming_space[incoming_space_index].lock_incoming_space_info));
            //            
            
            // Global drone arraydeki drone bilgilerini güncelliyoruz. Hub görebilsin diye.    
            sem_wait(&(drone_array[drone_id-1].lock_drone_info));
            drone_array[drone_id-1].current_range = current_range;
            drone_array[drone_id-1].is_active = false;
            sem_post(&(drone_array[drone_id-1].lock_drone_info));
            //
        }
        else if (nearby_hub_request_flag) {    
            int distance = hub_array[current_hub_id-1].distance_to_other_hubs[(drone_array[drone_id-1].nearby_hub_id)-1]; 
            int destination_hub_id = drone_array[drone_id-1].nearby_hub_id;
            
            // Reserve charging space at the destination hub.
            bool flag = true; 
            while (flag) {
                for (int i=0;i<hub_array[destination_hub_id-1].init_charging_space_length;i++) {
                    sem_wait(&(hub_array[destination_hub_id-1].hub_charging_space[i].lock_charging_space_info));
                    if (hub_array[destination_hub_id-1].hub_charging_space[i].is_empty) {
                        hub_array[destination_hub_id-1].hub_charging_space[i].is_empty = false;
                        hub_array[destination_hub_id-1].hub_charging_space[i].drone_id = drone_id;
                        flag=false;
                        sem_post(&(hub_array[destination_hub_id-1].hub_charging_space[i].lock_charging_space_info));
                        break;
                    }
                    sem_post(&(hub_array[destination_hub_id-1].hub_charging_space[i].lock_charging_space_info));
                }
            }
            
            
            current_range = calculate_drone_charge(timeInMilliseconds()-drone_parked_time,current_range,max_range);
            // Current Range is low. Wait for charge.
            if ((distance/drone_speed) > current_range) {
                waiting_time = ((distance/drone_speed)-current_range) * UNIT_TIME;
                wait(waiting_time);
                current_range = calculate_drone_charge(waiting_time,current_range,max_range);
            }

            FillDroneInfo(&drone_info,drone_id,current_hub_id,current_range,NULL,destination_hub_id);
            WriteOutput(NULL,NULL,&drone_info,NULL,DRONE_GOING);  

            // Release charging space.
            for (int i=0;i<hub_array[current_hub_id-1].init_charging_space_length;i++) {
                sem_wait(&(hub_array[current_hub_id-1].hub_charging_space[i].lock_charging_space_info));
                if (hub_array[current_hub_id-1].hub_charging_space[i].drone_id == drone_id) {
                    hub_array[current_hub_id-1].hub_charging_space[i].is_empty = true;
                    sem_post(&(hub_array[current_hub_id-1].hub_charging_space[i].lock_charging_space_info));
                    break;
                }
                sem_post(&(hub_array[current_hub_id-1].hub_charging_space[i].lock_charging_space_info));
            }   

      
            travel(distance,drone_speed);
            current_range = current_range - range_decrease(distance,drone_speed);
            current_hub_id = destination_hub_id;            

            drone_parked_time = timeInMilliseconds();

            FillDroneInfo(&drone_info,drone_id,current_hub_id,current_range,NULL,0);
            WriteOutput(NULL,NULL,&drone_info,NULL,DRONE_ARRIVED);

            // Signal to waiting hub. Drone arrived. Do not change is active to false.
            // Active false yapsak başkası kapabilir. O yüzden active true hala. 
            // Bizi bekleyen hub direkt biz gelince paketimizi verecek. Öyle yazdık hubı.
            sem_wait(&(drone_array[drone_id-1].lock_drone_info));
            drone_array[drone_id-1].current_range = current_range;
            drone_array[drone_id-1].drone_come_to_nearby_hub = true;    
            drone_array[drone_id-1].is_active = true;
            drone_array[drone_id-1].nearby_hub_request = false;
            drone_array[drone_id-1].transport_package = false;
            sem_post(&(drone_array[drone_id-1].lock_drone_info));
            //
        }
    }



    // Ne olur ne olmaz aktifi true yapıp çıkalım. Bize karışmazlar aktif true iken.
    sem_wait(&(drone_array[drone_id-1].lock_drone_info));
    drone_array[drone_id-1].is_active = true;
    FillDroneInfo(&drone_info,drone_id,current_hub_id,current_range,NULL,0);
    WriteOutput(NULL,NULL,&drone_info,NULL,DRONE_STOPPED);
    sem_post(&(drone_array[drone_id-1].lock_drone_info));

    return;
}


void hub_thread(void* id) {
    int hub_id = *((int *) id); 
    free(id);
    HubInfo hub_info;
    FillHubInfo(&hub_info,hub_id);
    WriteOutput(NULL,NULL,NULL,&hub_info,HUB_CREATED);
    long long waiting_time = UNIT_TIME;
    bool there_are_senders = true;
    bool there_are_outgoing_storage = true;
    bool there_are_incoming_storage = true;
    int error_no;
    int sem_val;
    int check_turn = 0;
    sem_wait(&(lock_barrier_for_sender));
    barrier_for_sender = barrier_for_sender + 1;
    sem_post(&(lock_barrier_for_sender));
    // Waiting for entering number of drones info.
    while (true) {
        if (number_of_drones_at_the_beginning) break;   
    }

    // Waiting for all drone threads created.
    while (true) {
        sem_wait(&(lock_current_number_of_drones));
        if (current_number_of_drones == number_of_drones_at_the_beginning) {    
            sem_post(&(lock_current_number_of_drones));
            break;
        }
        sem_post(&(lock_current_number_of_drones));
    }


    while (there_are_senders || there_are_outgoing_storage || there_are_incoming_storage) {
        sem_wait(&(lock_current_number_of_senders));
        if (current_number_of_senders <= 0) {
            there_are_senders = false;
        }
        sem_post(&(lock_current_number_of_senders));
        if (there_are_senders || there_are_outgoing_storage) {
            int outgoing_storage_usage = 0;
            for (int i=0;i<hub_array[hub_id-1].init_outgoing_space_length;i++) {                                 
                sem_wait(&(hub_array[hub_id-1].hub_outgoing_space[i].lock_outgoing_space_info)); //Outgoing Space Lockla boş olup olmadığına bakıcaz.               // İlk for loopta outgoing space bir şey gelmiş mi ona bakıyoruz.                                      
                if (!(hub_array[hub_id-1].hub_outgoing_space[i].is_empty) && !(hub_array[hub_id-1].hub_outgoing_space[i].is_drone_take_this_space)) {               // İkinci for loopta charging space dolu mu ona bakıyoruz. 
                    outgoing_storage_usage ++;
                    sem_post(&(hub_array[hub_id-1].hub_outgoing_space[i].lock_outgoing_space_info)); // Outgoing space unlock. Dolu çıktı, drone boşaltacak.        // Üçüncü olarak ta drone aktif mi ona bakıyoruz.                     
                    int highest_range = -10000;
                    int drone_index_for_package = -1;                                                                                                           // Bulduğumuz en yüksek range' li drone' a outgoing space' deki paketi veriyoruz.                                 
                        start:
                            highest_range = -10000;
                            drone_index_for_package = -1;
                            for (int j=0;j<hub_array[hub_id-1].init_charging_space_length;j++) {
                                sem_wait(&(hub_array[hub_id-1].hub_charging_space[j].lock_charging_space_info)); // Charging Space Lockla boş olup olmadığına bakıcaz.
                                if (!(hub_array[hub_id-1].hub_charging_space[j].is_empty)) {  
                                    sem_wait(&(drone_array[hub_array[hub_id-1].hub_charging_space[j].drone_id-1].lock_drone_info)); // Drone lock, charging space dolu. Drone aktif mi ona bakıyoruz.  
                                    if (!(drone_array[hub_array[hub_id-1].hub_charging_space[j].drone_id-1].is_active)) {
                                        if (drone_array[hub_array[hub_id-1].hub_charging_space[j].drone_id-1].current_range > highest_range) {
                                            highest_range = drone_array[hub_array[hub_id-1].hub_charging_space[j].drone_id-1].current_range;
                                            if (drone_index_for_package != -1) {
                                                sem_post(&(drone_array[drone_index_for_package].lock_drone_info)); // Unlock previous. -1 check yapıyoruz. İlk drone özel durum var çünkü.
                                            }
                                            drone_index_for_package = hub_array[hub_id-1].hub_charging_space[j].drone_id-1;     // Yeni drone indexini gir. Öncekini unlockla.
                                        }  
                                        else {
                                            sem_post(&(drone_array[hub_array[hub_id-1].hub_charging_space[j].drone_id-1].lock_drone_info)); // Range highesttan düşük. Direkt unlock drone. 
                                        }
                                    }   
                                    else {
                                        sem_post(&(drone_array[hub_array[hub_id-1].hub_charging_space[j].drone_id-1].lock_drone_info)); // Drone unlock. Aktif zaten, şimdi kullanamayız.
                                    }
                                }           
                                sem_post(&(hub_array[hub_id-1].hub_charging_space[j].lock_charging_space_info)); // Charging space unlock. Boş olup olmadığına baktık.                                                                   
                            }

                            // We have found 'not active drone' in our charging space and we have package. Let's assign package to drone.
                            if (drone_index_for_package != -1) {
                                // Bu paketi bir drone ' a verdik dememiz lazım. Birden fazla drone'u aynı pakete vermemek için.
                                sem_wait(&(hub_array[hub_id-1].hub_outgoing_space[i].lock_outgoing_space_info));
                                hub_array[hub_id-1].hub_outgoing_space[i].is_drone_take_this_space = true;
                                sem_post(&(hub_array[hub_id-1].hub_outgoing_space[i].lock_outgoing_space_info));

                                // Loopta wait yapmıştık zaten bu drone'a . Kontrolü bizde hala yani.
                                // O yüzden şimdi direkt bilgilerini doldurup post yapıcaz.
                                drone_array[drone_index_for_package].is_active = true;
                                drone_array[drone_index_for_package].transport_package = true;
                                drone_array[drone_index_for_package].nearby_hub_request = false;
                                drone_array[drone_index_for_package].release_outgoing_space_index = i;
                                drone_array[drone_index_for_package].package->sender_id = hub_array[hub_id-1].hub_outgoing_space[i].package->sender_id;
                                drone_array[drone_index_for_package].package->sending_hub_id = hub_array[hub_id-1].hub_outgoing_space[i].package->sending_hub_id;
                                drone_array[drone_index_for_package].package->receiver_id = hub_array[hub_id-1].hub_outgoing_space[i].package->receiver_id;
                                drone_array[drone_index_for_package].package->receiving_hub_id = hub_array[hub_id-1].hub_outgoing_space[i].package->receiving_hub_id;
                                sem_post(&(drone_array[drone_index_for_package].lock_drone_info));   
                            }

                            // We have not found any drone. So we will steal other hubs drone.
                            else {
                                // Get minimum indexes. We will try to find from closest hub.
                                int* min_indexes = get_min_elements_index_array(hub_array[hub_id-1].distance_to_other_hubs,number_of_hubs_at_the_beginning);
                                bool flag_for_finding_drone = false;
                                int found_drone_index = -1;
                                for (int i=0;i<number_of_hubs_at_the_beginning;i++) {
                                    int nearby_hub_id = min_indexes[i]+1;
                                    if (nearby_hub_id == hub_id) continue; 
                                    else { 
                                        sem_wait(&(hub_array[nearby_hub_id-1].lock_hub_info));
                                        if (hub_array[nearby_hub_id-1].is_active) {
                                            sem_post(&(hub_array[nearby_hub_id-1].lock_hub_info));  // Nearby hub active. We will try to find available drones.
                                            int highest_range = -10000;
                                            int drone_index_for_package = -1;
                                            for (int j=0;j<hub_array[nearby_hub_id-1].init_charging_space_length;j++) {
                                                sem_wait(&(hub_array[nearby_hub_id-1].hub_charging_space[j].lock_charging_space_info)); // Charging Space Lockla boş olup olmadığına bakıcaz. 
                                                if (!(hub_array[nearby_hub_id-1].hub_charging_space[j].is_empty)) {    
                                                    sem_wait(&(drone_array[hub_array[nearby_hub_id-1].hub_charging_space[j].drone_id-1].lock_drone_info)); // Drone lock, charging space dolu. Drone aktif mi ona bakıyoruz.  
                                                    if (!(drone_array[hub_array[nearby_hub_id-1].hub_charging_space[j].drone_id-1].is_active)) {
                                                        if (drone_array[hub_array[nearby_hub_id-1].hub_charging_space[j].drone_id-1].current_range > highest_range) {
                                                            highest_range = drone_array[hub_array[nearby_hub_id-1].hub_charging_space[j].drone_id-1].current_range;
                                                            if (drone_index_for_package != -1) {
                                                                sem_post(&(drone_array[drone_index_for_package].lock_drone_info)); // Unlock previous. -1 check yapıyoruz. İlk drone özel durum var çünkü.
                                                            }
                                                            drone_index_for_package = hub_array[nearby_hub_id-1].hub_charging_space[j].drone_id-1;     // Yeni range ve index gir. Öncekini unlockla.
                                                        }  
                                                        else {
                                                            sem_post(&(drone_array[hub_array[nearby_hub_id-1].hub_charging_space[j].drone_id-1].lock_drone_info)); // Range highesttan düşük. Direkt unlock drone. 
                                                        }
                                                    }   
                                                    else {
                                                        sem_post(&(drone_array[hub_array[nearby_hub_id-1].hub_charging_space[j].drone_id-1].lock_drone_info)); // Drone unlock. Aktif zaten, şimdi kullanamayız.
                                                    }
                                                }           
                                                sem_post(&(hub_array[nearby_hub_id-1].hub_charging_space[j].lock_charging_space_info)); // Charging space unlock. Boş olup olmadığına baktık.                        
                                            }
                                            if (drone_index_for_package != -1) {
                                                // We have found 'not active drone' in nearby charging space and. Let's call to drone.
                                                // For loopta wait yapmıştık. O yüzden direkt çağırıp postluyoruz.
                                                // Çağırdığımızı bekliyoruz looptan çıkıp.
                                                drone_array[drone_index_for_package].is_active = true;
                                                drone_array[drone_index_for_package].transport_package = false;
                                                drone_array[drone_index_for_package].nearby_hub_request = true;
                                                drone_array[drone_index_for_package].drone_come_to_nearby_hub = false;
                                                drone_array[drone_index_for_package].nearby_hub_id = hub_id;
                                                sem_post(&(drone_array[drone_index_for_package].lock_drone_info));  
                                                flag_for_finding_drone = true; 
                                                found_drone_index = drone_index_for_package;
                                                break;   
                                            }
                                        }
                                        else {
                                            sem_post(&(hub_array[nearby_hub_id-1].lock_hub_info));
                                        }
                                    }
                                }
                                free(min_indexes);
                                if (flag_for_finding_drone) {
                                    while (true) {
                                        // Found drone index üzerinden sürekli bakıyoruz gelmiş mi diye. Gelince paketi yükleyip gönderecez.
                                        sem_wait(&(drone_array[found_drone_index].lock_drone_info));
                                        if (drone_array[found_drone_index].drone_come_to_nearby_hub) {
                                            sem_wait(&(hub_array[hub_id-1].hub_outgoing_space[i].lock_outgoing_space_info));
                                            hub_array[hub_id-1].hub_outgoing_space[i].is_drone_take_this_space = true;
                                            sem_post(&(hub_array[hub_id-1].hub_outgoing_space[i].lock_outgoing_space_info));
                                            drone_array[found_drone_index].is_active = true;
                                            drone_array[found_drone_index].transport_package = true;
                                            drone_array[found_drone_index].nearby_hub_request = false;
                                            drone_array[found_drone_index].drone_come_to_nearby_hub = false;
                                            drone_array[found_drone_index].release_outgoing_space_index = i;
                                            drone_array[found_drone_index].package->sender_id = hub_array[hub_id-1].hub_outgoing_space[i].package->sender_id;
                                            drone_array[found_drone_index].package->sending_hub_id = hub_array[hub_id-1].hub_outgoing_space[i].package->sending_hub_id;
                                            drone_array[found_drone_index].package->receiver_id = hub_array[hub_id-1].hub_outgoing_space[i].package->receiver_id;
                                            drone_array[found_drone_index].package->receiving_hub_id = hub_array[hub_id-1].hub_outgoing_space[i].package->receiving_hub_id;
                                            sem_post(&(drone_array[found_drone_index].lock_drone_info));     
                                            break;
                                        }
                                        sem_post(&(drone_array[found_drone_index].lock_drone_info));
                                    }
                                }
                                else {    
                                    wait(waiting_time);    // We did not find drone. Wait specific time and try again
                                    goto start;
                                }
                            }
                }
                else {
                    sem_post(&(hub_array[hub_id-1].hub_outgoing_space[i].lock_outgoing_space_info)); // Outgoing space unlock. Boş çıktı, devam.
                }
            }
            if (!there_are_senders && outgoing_storage_usage == 0) {
                there_are_outgoing_storage = false;
            }
        }

        else if (!there_are_senders && !there_are_outgoing_storage && there_are_incoming_storage) {
            sem_wait(&(lock_number_of_sended_packages));
            if (number_of_sended_packages == 0) {
                there_are_incoming_storage = false;
            }
            sem_post(&(lock_number_of_sended_packages));
        }
    }



    // Hubı kapatıyoruz.
    sem_wait(&(hub_array[hub_id-1].lock_hub_info));
    hub_array[hub_id-1].is_active = false;
    sem_post(&(hub_array[hub_id-1].lock_hub_info));
    //
    
    // Hub sayısını bir azaltıyoruz.
    sem_wait(&(lock_current_number_of_hubs));
    current_number_of_hubs = current_number_of_hubs - 1; 
    FillHubInfo(&hub_info,hub_id);
    WriteOutput(NULL,NULL,NULL,&hub_info,HUB_STOPPED);
    sem_post(&(lock_current_number_of_hubs));
    //

    return;
}



int main() {
    pthread_t* threads_for_hubs;
    pthread_t* threads_for_senders;
    pthread_t* threads_for_receivers;
    pthread_t* threads_for_drones;
    //  Take input and initialize global variables. 
    scanf("%d",&number_of_hubs_at_the_beginning);
    threads_for_hubs = (pthread_t *) malloc(number_of_hubs_at_the_beginning*sizeof(pthread_t));
    threads_for_senders = (pthread_t *) malloc(number_of_hubs_at_the_beginning*sizeof(pthread_t));
    threads_for_receivers = (pthread_t *) malloc(number_of_hubs_at_the_beginning*sizeof(pthread_t));
    hub_array = (hub *) malloc(number_of_hubs_at_the_beginning*sizeof(hub));
    sender_hubs = (int *) malloc(number_of_hubs_at_the_beginning*sizeof(int));
    receiver_hubs = (int *) malloc(number_of_hubs_at_the_beginning*sizeof(int));
    current_number_of_senders = number_of_hubs_at_the_beginning;
    current_number_of_hubs = number_of_hubs_at_the_beginning;
    number_of_drones_at_the_beginning = 0;
    current_number_of_drones = 0;
    current_number_of_receivers = 0;
    barrier_for_sender = 0;
    number_of_sended_packages = 0;
    int incoming_package_size,outgoing_package_size,charging_space_size;
    int error_no;
    sem_init(&lock_current_number_of_senders,0,1); 
    sem_init(&lock_current_number_of_hubs,0,1);
    sem_init(&(lock_current_number_of_drones),0,1);
    sem_init(&(lock_current_number_of_receivers),0,1);
    sem_init(&(lock_barrier_for_sender),0,1);
    sem_init(&(lock_number_of_sended_packages),0,1);
    InitWriteOutput();
    for (int i=0;i<number_of_hubs_at_the_beginning;i++) {
        hub_array[i].hub_id = i+1;
        hub_array[i].is_active = true;
        sem_init(&(hub_array[i].lock_hub_info),0,1); 
        scanf("%d %d %d",&incoming_package_size,&outgoing_package_size,&charging_space_size);
        // Allocate space for hub spaces.
        hub_array[i].hub_incoming_space = (incoming_space *) malloc(incoming_package_size*sizeof(incoming_space)); 
        hub_array[i].hub_outgoing_space = (outgoing_space *) malloc(outgoing_package_size*sizeof(outgoing_space)); 
        hub_array[i].hub_charging_space = (charging_space *) malloc(charging_space_size*sizeof(charging_space));   
        hub_array[i].distance_to_other_hubs = (int *) malloc(number_of_hubs_at_the_beginning*sizeof(int));
        hub_array[i].init_outgoing_space_length = outgoing_package_size;
        hub_array[i].init_incoming_space_length = incoming_package_size;
        hub_array[i].init_charging_space_length = charging_space_size;

        // Initialize incoming spaces to empty.
        for (int k=0;k<incoming_package_size;k++) {
            hub_array[i].hub_incoming_space[k].is_drone_arrived = false;
            hub_array[i].hub_incoming_space[k].is_empty = true;
            sem_init(&(hub_array[i].hub_incoming_space[k].lock_incoming_space_info),0,1);
            hub_array[i].hub_incoming_space[k].package = (PackageInfo *) malloc(sizeof(PackageInfo));
        }                                                      

        // Initialize outgoing spaces to empty.
        for (int k=0;k<outgoing_package_size;k++) {
            hub_array[i].hub_outgoing_space[k].is_empty = true;
            hub_array[i].hub_outgoing_space[k].is_drone_take_this_space = false;
            sem_init(&(hub_array[i].hub_outgoing_space[k].lock_outgoing_space_info),0,1);      
            hub_array[i].hub_outgoing_space[k].package = (PackageInfo *) malloc(sizeof(PackageInfo));
        } 


        // Initialize charging spaces to empty.
        for (int k=0;k<charging_space_size;k++) {
            hub_array[i].hub_charging_space[k].is_empty = true;
            sem_init(&(hub_array[i].hub_charging_space[k].lock_charging_space_info),0,1);       
        }                                                                               

        // Take distances to other hubs and save them.
        for (int j=0;j<number_of_hubs_at_the_beginning;j++) {
            int distance;
            scanf("%d",&distance);
            hub_array[i].distance_to_other_hubs[j] = distance;
        }
        int* hub_id_as_pointer = (int*) malloc(sizeof(int));
        *hub_id_as_pointer = i+1; 
        if ((error_no = pthread_create(threads_for_hubs+i,NULL,hub_thread,(void *) hub_id_as_pointer)) !=0) {    
        }
    }

    for (int i=0;i<number_of_hubs_at_the_beginning;i++) {
        int sender_waiting_time,sender_assigned_hub_id,number_of_packages;
        scanf("%d %d %d",&sender_waiting_time,&sender_assigned_hub_id,&number_of_packages);
        number_of_sended_packages = number_of_sended_packages + number_of_packages;
        int* sender_arguments = (int *) malloc(4*sizeof(int));
        sender_arguments[0] = i+1;
        sender_arguments[1] = sender_waiting_time;
        sender_arguments[2] = number_of_packages;
        sender_arguments[3] = sender_assigned_hub_id;
        sender_hubs[i] = sender_assigned_hub_id;
        if ((error_no = pthread_create(threads_for_senders+i,NULL,sender_thread,(void *) sender_arguments)) != 0) {
        }
    }

    for (int i=0;i<number_of_hubs_at_the_beginning;i++) {
        int receiver_waiting_time,receiver_assigned_hub_id;
        scanf("%d %d",&receiver_waiting_time,&receiver_assigned_hub_id);
        int* receiver_arguments = (int *) malloc(3*sizeof(int));
        receiver_arguments[0] = i+1;
        receiver_arguments[1] = receiver_waiting_time;
        receiver_arguments[2] = receiver_assigned_hub_id;
        receiver_hubs[i] = receiver_assigned_hub_id;
        if ((error_no = pthread_create(threads_for_receivers+i,NULL,receiver_thread,(void *) receiver_arguments)) != 0) {
        }        
    }

    scanf("%d",&number_of_drones_at_the_beginning);
    threads_for_drones = (pthread_t *) malloc(number_of_drones_at_the_beginning*sizeof(pthread_t));
    drone_array = (drone *) malloc(number_of_drones_at_the_beginning*sizeof(drone));
    for (int i=0;i<number_of_drones_at_the_beginning;i++) {
        int drone_travel_speed,drone_starting_hub_id,drone_maximum_range;
        scanf("%d %d %d",&drone_travel_speed,&drone_starting_hub_id,&drone_maximum_range);
        drone_array[i].drone_id = i+1;
        drone_array[i].is_active = false;
        drone_array[i].transport_package = false;
        drone_array[i].nearby_hub_request = false;
        sem_init(&(drone_array[i].lock_drone_info),0,1);
        drone_array[i].current_range = drone_maximum_range;
        drone_array[i].package = (PackageInfo *) malloc(sizeof(PackageInfo));
        drone_array[i].release_outgoing_space_index = -1;
        drone_array[i].drone_come_to_nearby_hub = false;
        int* drone_arguments = (int *) malloc(4*sizeof(int));
        drone_arguments[0] = i+1;
        drone_arguments[1] = drone_travel_speed;
        drone_arguments[2] = drone_starting_hub_id;
        drone_arguments[3] = drone_maximum_range;
        if ((error_no = pthread_create(threads_for_drones+i,NULL,drone_thread,(void *) drone_arguments)) != 0) {
        }          

    }



    for (int i=0;i<number_of_hubs_at_the_beginning;i++) {
        pthread_join(threads_for_hubs[i],NULL);
    }
    
    for (int i=0;i<number_of_hubs_at_the_beginning;i++) {
        pthread_join(threads_for_senders[i],NULL);
    }
   
    for (int i=0;i<number_of_hubs_at_the_beginning;i++) {
        pthread_join(threads_for_receivers[i],NULL);
    }
    
    for (int i=0;i<number_of_drones_at_the_beginning;i++) {
        pthread_join(threads_for_drones[i],NULL);
    }


    for (int j=0;j<number_of_hubs_at_the_beginning;j++) {
        int hub_id = j+1;
        for (int i=0;i<hub_array[hub_id-1].init_outgoing_space_length;i++) {
            free(hub_array[hub_id-1].hub_outgoing_space[i].package);
            sem_destroy(&(hub_array[hub_id-1].hub_outgoing_space[i].lock_outgoing_space_info));
        }
    }
    for (int j=0;j<number_of_hubs_at_the_beginning;j++) {
        int hub_id = j+1;
        for (int i=0;i<hub_array[hub_id-1].init_incoming_space_length;i++) {
        free(hub_array[hub_id-1].hub_incoming_space[i].package);
        sem_destroy(&(hub_array[hub_id-1].hub_incoming_space[i].lock_incoming_space_info));
        }
    }
    for (int j=0;j<number_of_hubs_at_the_beginning;j++) {
        int hub_id = j+1;
        for (int i=0;i<hub_array[hub_id-1].init_charging_space_length;i++) {
            sem_destroy(&(hub_array[hub_id-1].hub_charging_space[i].lock_charging_space_info));
        }
    }
    for (int j=0;j<number_of_hubs_at_the_beginning;j++) {
        int hub_id = j+1;
        free(hub_array[hub_id-1].distance_to_other_hubs);
        free(hub_array[hub_id-1].hub_charging_space);
        free(hub_array[hub_id-1].hub_incoming_space);
        free(hub_array[hub_id-1].hub_outgoing_space);
        sem_destroy(&(hub_array[hub_id-1].lock_hub_info));
    }
    for (int j=0;j<number_of_drones_at_the_beginning;j++) {
        sem_destroy(&(drone_array[j].lock_drone_info));
        free(drone_array[j].package);
    }


    free(hub_array);
    free(drone_array);
    free(threads_for_hubs);
    free(threads_for_senders);
    free(threads_for_receivers);
    free(threads_for_drones);
    free(receiver_hubs);
    free(sender_hubs);
    sem_destroy(&lock_current_number_of_hubs);
    sem_destroy(&lock_current_number_of_senders);
    sem_destroy(&(lock_current_number_of_drones));
    sem_destroy(&(lock_current_number_of_receivers));
    sem_destroy(&(lock_barrier_for_sender));
    sem_destroy(&(lock_number_of_sended_packages));
    return 0;
}
