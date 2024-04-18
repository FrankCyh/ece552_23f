#include "predictor.h"
#include "vector"
#include <fstream>
#include <cmath>
/////////////////////////////////////////////////////////////
// 2bitsat
/////////////////////////////////////////////////////////////
int GHR_size = 50;
vector<double> GHR(GHR_size, 0);
//twobitSat opened(2560);
vector<uint64_t> power_table(11);
int two_bit_length = 2048;
vector<vector<int>> local_history(1024, vector<int>(11, 0));
int find_local_history_entry_index(UINT32 PC) {
    PC &= 0x3FF;
    int temp = static_cast<int>(PC);
    int index_of_local_history = temp % 1024;
    return index_of_local_history;
}
class twobitSat {

public:
    
    vector<vector<int>> arr;
    //2bitSat(int num_of_indice) {
    //    arr.resize(num_of_indice, vector<int>(2));
    //}
    twobitSat(int num_of_indices) {
        arr.resize(num_of_indices, std::vector<int>(2,0));
    }

    int give_index(UINT32 PC) {
        int index = PC % 4096;
        return index;
    }
    
    int give_index_open(UINT32 PC) {
	int index_into_history = find_local_history_entry_index(PC);
	int temp = 0;
        for (int i = 0; i < 11; i++) {
	    temp += local_history[index_into_history][i] * power_table[i];
	}
	return temp % 2048;
	//int index = PC % 2048;
	//return index;
	/*PC &= 0x7FF;
        int32_t temp = static_cast<int>(PC); 
        temp = temp % two_bit_length;
        if (temp < 0) {
            temp += two_bit_length;
        }   
        return temp;*/ 					//This Line has been changed/
        /*int sum = 
        for (int i = 0; i < 12; i++) {
            sum += GHR[38+i] * power_table[i];
        }
           
        return sum % 2560; 
*/	/*int temp = 0;
	int mask = 0x7FF;
        for (int i = 44; i < 55; i++) {
            temp = temp ^ (int)GHR[i];
	    temp = temp << 1;
	}
	temp = temp >> 1;
	
        mask = mask & (int)PC;
	temp = temp ^ mask;
	return temp % 2048;*/
    }

};
 
twobitSat record(4096);

void InitPredictor_2bitsat() {
    
    //record.arr.resize(4096, vector<int>(2,0));
    
}

bool GetPrediction_2bitsat(UINT32 PC) {
    int index = record.give_index(PC);
    if (!record.arr[index][0]){          // T = 11; t = 10; n = 01; N = 00
        return NOT_TAKEN;
    }
        return TAKEN;

}

void UpdatePredictor_2bitsat(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
    int index = record.give_index(PC);
    if (resolveDir == predDir) {
        if (predDir == TAKEN) {    // I wanted to use binary representation such as 0b00, so I only need to add 1 to all case; but that might be confusing to debug.
            int first_bit = record.arr[index][0]; // First and second from left to right
            int second_bit = record.arr[index][1];
            if (first_bit && !second_bit) { // t -> TT
		record.arr[index][1] = 1;
            }
        } else {
	    int first_bit = record.arr[index][0]; // First and second from left to right
            int second_bit = record.arr[index][1];
            if (!first_bit && second_bit) { // n -> N
                record.arr[index][1] = 0;
            }   
	}
   } else {
      if (predDir == TAKEN) {    // I wanted to use binary representation such as 0b00, so I only need to add 1 to all case; but that might be confusing to debug.
            //int first_bit = record.arr[index][0]; // First and second from left to right
            int second_bit = record.arr[index][1];
            if (second_bit) {
	        record.arr[index][1] = 0; // T - > t
            } else {
                record.arr[index][0] = 0;
                record.arr[index][1] = 1; // t -> n 
	    }   
      } else {
            //int first_bit = record.arr[index][0]; // First and second from left to right
            int second_bit = record.arr[index][1];
            if (second_bit) { // n -> t
	        record.arr[index][0] = 1;
                record.arr[index][1] = 0;
	    } else {
                record.arr[index][1] = 1;
	    }

      }
  
   } 
}

/////////////////////////////////////////////////////////////
// 2level
/////////////////////////////////////////////////////////////
int BHT_num = 9; // 2^9 = 512       11:3
int PHT_entry_per_table = 64; // 2^6 = 64
int PHT_table_num = 3; // 2^3 = 8   2:0

class BHT {

public:

    vector<vector<int>> arr;
 
    int give_index(UINT32 PC) {
        int startBit = 3;
        int endBit = 11;
        uint32_t mask = ((1 << (endBit - startBit + 1)) - 1) << startBit;
        uint32_t extractedBits = (PC & mask) >> startBit;
        int index = extractedBits % 512;
        return index;
    }

};

class PHT {

public:
    
    vector<vector<string>> arr; // Use string with length == 2 to represent - ignore

    int give_row_index(int sixbit_table1_output) {
        int index = sixbit_table1_output % 64;
        return index;
    }

    int give_row_index_op(int PC) {
        int startBit = 3;
        int endBit = 8;
        uint32_t mask = ((1 << (endBit - startBit + 1)) - 1) << startBit;
        uint32_t extractedBits = (PC & mask) >> startBit;
        int index = extractedBits % 64;
        return index;    
    }

    int give_table_index(UINT32 PC) {
        int startBit = 0;
        int endBit = 2;
        uint32_t mask = ((1 << (endBit - startBit + 1)) - 1) << startBit;
        uint32_t extractedBits = (PC & mask) >> startBit;
        int index = extractedBits % 8;
	return index;
    }

};

BHT first_level_table;
PHT second_level_table;

void InitPredictor_2level() {
    first_level_table.arr.resize(512, vector<int>(6,0));
    second_level_table.arr.resize(64, vector<string>(8, "01")); // T = 11; t = 10; n = 01; N = 00;
}


bool GetPrediction_2level(UINT32 PC) {
    
    int index = first_level_table.give_index(PC);
    vector<int> temp = first_level_table.arr[index];
    int value = temp[0]*32 + temp[1]*16 + temp[2]*8 + temp[3]*4 + temp[4]*2 + temp[5]*1;
    int row_index_table2 = second_level_table.give_row_index(value);
    int table_index_table2 = second_level_table.give_table_index(PC);
    
    if (second_level_table.arr[row_index_table2][table_index_table2][0] == '0'){
        return NOT_TAKEN;       
    }
    return TAKEN;
}
void first_level_shift(bool resolveDir, int index) {
    int size = first_level_table.arr[index].size();
    for (int i = 0 ; i < size - 1; i++) {
        first_level_table.arr[index][i] = first_level_table.arr[index][i+1];    
    }
    first_level_table.arr[index][size-1] = (int)resolveDir;
}

void UpdatePredictor_2level(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
    
    int first_level_index = first_level_table.give_index(PC);
    vector<int> temp = first_level_table.arr[first_level_index];
    int value = temp[0]*32 + temp[1]*16 + temp[2]*8 + temp[3]*4 + temp[4]*2 + temp[5]*1;
    int row_index_table2 = second_level_table.give_row_index(value);
    int table_index_table2 = second_level_table.give_table_index(PC);
    
    if (predDir == resolveDir) {
        if (predDir == TAKEN) {
	    first_level_shift(true, first_level_index);
	    if (second_level_table.arr[row_index_table2][table_index_table2] == "10") {
	        second_level_table.arr[row_index_table2][table_index_table2] = "11";
	    }
        } else {
	    first_level_shift(false, first_level_index);
	    if (second_level_table.arr[row_index_table2][table_index_table2] == "01") {
                second_level_table.arr[row_index_table2][table_index_table2] = "00";
            } 
	}
    } else {
	if (predDir == TAKEN) {
            first_level_shift(false, first_level_index); // T = 11; t = 10; n = 01; N = 00;
            if(second_level_table.arr[row_index_table2][table_index_table2] == "11") {
	        second_level_table.arr[row_index_table2][table_index_table2] = "10";
	    } else {
                second_level_table.arr[row_index_table2][table_index_table2] = "01";
            }
        } else {
	    first_level_shift(true, first_level_index); // T = 11; t = 10; n = 01; N = 00;
            if(second_level_table.arr[row_index_table2][table_index_table2] == "01") {
                second_level_table.arr[row_index_table2][table_index_table2] = "10";
            } else {
                second_level_table.arr[row_index_table2][table_index_table2] = "01";
            }
        }
    }
}

/////////////////////////////////////////////////////////////
// openend
/////////////////////////////////////////////////////////////
//vector<double> GHR(50, 0);
twobitSat opened(two_bit_length);

//vector<uint64_t> power_table(12);
class Perceptron {

public:
    double bias;
    vector<vector<double>> weights;
    //double bias;
    /*Perceptron() {
        bias = 0.0;
        vector<vector<double>> weights(2048, vector<double>(50, 0.0));
    }*/
    Perceptron() : bias(0.0), weights(2048, vector<double>(GHR_size, 0.0)) {
    }

    int activation(int x) {
        return (x > 0) ? 1 : 0;
    }
    int findIndex(UINT32 PC) {
        PC &= 0x7FF;
        int temp = static_cast<int>(PC); 
        /*temp = temp % 2048;
        if (temp < 0) {
            temp += 2048;
        }
        return temp;
        */
	int sum = 0;
	//int temp = 0;
        for (int i = 0; i < 11; i++) {
	    // temp = (GHR[39+i] > 0) ? GHR[39+i] : GHR[39+i]*-1;
            sum += GHR[GHR_size - 11 + i] * power_table[i];
	}
	sum = sum ^ temp;
        return sum % 2048;
        //return temp;
    }

    int predict(UINT32 PC) {
        double sum = 0;
        int index = this->findIndex(PC); // Find Index of Table
	for (int i = 0; i < GHR_size; ++i) {
            sum += weights[index][i] * GHR[i];
        }
        return activation(static_cast<int>(sum + bias));
    }

    void train(UINT32 PC, int label, double learning_rate = 1) {
        /*double sum = 0;
        int index = this->findIndex(PC); // Find Index of Table
        for (int i = 0; i < 50; ++i) {
            sum += weights[index][i] * GHR[i];
        }*/
        //if (sum <= 110.5) {
            int prediction = this->predict(PC);
            int error = label - prediction;
            int index = this->findIndex(PC);
            for (int i = 0; i < GHR_size; ++i) {
                weights[index][i] += learning_rate * error * GHR[i];
            }
            bias += learning_rate * error;
        //}
    }
/*    void saveWeightsToFile(const std::string& filename) {
        std::ofstream file(filename, std::ios::out | std::ios::binary);

        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(&weights[0]), weights.size() * sizeof(double));
            file.close();
        }
    }
    void loadWeightsFromFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);  // Include std::ios::binary for binary mode

        if (file.is_open()) {
            file.read(reinterpret_cast<char*>(&weights[0]), weights.size() * sizeof(double));
            file.close();
            std::cout << "Weights loaded from " << filename << std::endl;
        } else {
            std::cerr << "Unable to open file for reading." << std::endl;
        }
    }*/
    void saveWeightsToFile(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);

        if (file.is_open()) {
            // Write the weights matrix to the file
            for (const auto& row : weights) {
                file.write(reinterpret_cast<const char*>(row.data()), row.size() * sizeof(double));
            }

            // Write the bias
            file.write(reinterpret_cast<const char*>(&bias), sizeof(double));

            file.close();
            std::cout << "Weights saved to " << filename << std::endl;
        } else {
            std::cerr << "Unable to open file for writing." << std::endl;
        }
    }

    // Function to load weights from a file
    void loadWeightsFromFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);

        if (file.is_open()) {
            // Read the weights matrix from the file
            for (auto& row : weights) {
                file.read(reinterpret_cast<char*>(row.data()), row.size() * sizeof(double));
            }

            // Read the bias
            file.read(reinterpret_cast<char*>(&bias), sizeof(double));

            file.close();
            std::cout << "Weights loaded from " << filename << std::endl;
        } else {
            std::cerr << "Unable to open file for reading." << std::endl;
        }
    }
};
int count = 0;
Perceptron perceptron;
bool GetPrediction_2bitsat_open(UINT32 PC) {
    int index = opened.give_index_open(PC);
    if (!opened.arr[index][0]){          // T = 11; t = 10; n = 01; N = 00
        return NOT_TAKEN;
    }
        return TAKEN;

}

void UpdatePredictor_2bitsat_open(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
    int index = opened.give_index_open(PC);
    if (resolveDir == predDir) {
        if (predDir == TAKEN) {    // I wanted to use binary representation such as 0b00, so I only need to add 1 to all case; but that might be confusing to debug.
            int first_bit = opened.arr[index][0]; // First and second from left to right
            int second_bit = opened.arr[index][1];
            if (first_bit && !second_bit) { // t -> TT
		opened.arr[index][1] = 1;
            }
        } else {
	    int first_bit = opened.arr[index][0]; // First and second from left to right
            int second_bit = opened.arr[index][1];
            if (!first_bit && second_bit) { // n -> N
                opened.arr[index][1] = 0;
            }   
	}
   } else {
      if (predDir == TAKEN) {    // I wanted to use binary representation such as 0b00, so I only need to add 1 to all case; but that might be confusing to debug.
            //int first_bit = record.arr[index][0]; // First and second from left to right
            int second_bit = opened.arr[index][1];
            if (second_bit) {
	        opened.arr[index][1] = 0; // T - > t
            } else {
                opened.arr[index][0] = 0;
                opened.arr[index][1] = 1; // t -> n 
	    }   
      } else {
            //int first_bit = record.arr[index][0]; // First and second from left to right
            int second_bit = opened.arr[index][1];
            if (second_bit) { // n -> t
	        opened.arr[index][0] = 1;
                opened.arr[index][1] = 0;
	    } else {
                opened.arr[index][1] = 1;
	    }

      }
  
   }
   int temp = find_local_history_entry_index(PC);
   for (int i = 0; i < 10; i++) {
       local_history[temp][i] = local_history[temp][i+1]; // local_history is a table of 1024*11
   }
   local_history[temp][10] = (int)resolveDir;
  
}
int perceptron_correct = 0;
int twobit_correct = 0;
//unordered_map<UINT32, int> history;
struct MyData{
    UINT32 key;
    UINT32 value;
};
//vector<MyData> history(3);
void InitPredictor_openend() {
    //perceptron.loadWeightsFromFile("weights.bin");
    /*for (int i = 0 ; i < 3; i++) {
    	history[i].key = 0;
	history[i].value = 0;
    }*/
    for (int i = 0 ; i < 11; i++) {
    	power_table[i] = pow(2,10-i);
    }
}
int h_count = 0;
int num_of_competence = 0;
bool GetPrediction_openend(UINT32 PC) {
    /*vector<int> input(32, 0);

    for (int i = 31; i >= 0; --i) {
        input[i] = (PC & 1);
        PC >>= 1;
    }
    */
    /*int temp = static_cast<int>(PC & 0xFFF); 
    temp = temp % 100;
    if (temp < 0) {
        temp += 100;
    }    
    if (history[temp].key == PC) {
        return history[temp].value;
    }*/
    int prediction_p = perceptron.predict(PC);
    int prediction_t = GetPrediction_2bitsat_open(PC);
    if (prediction_p != prediction_t) {
        if (num_of_competence > 0) {
	    return bool(prediction_p);
	}
        if (num_of_competence < 0) {
	    return bool(prediction_t);
	}
        //if (history[h_count].key == PC) {
	//    return history[h_count].value < history[h_count].key;
	//}	
	if (perceptron_correct > twobit_correct) {
	    return bool(prediction_p);
	} else {
	    return bool(prediction_t);
	}
    }
    //int prediction = perceptron.predict(input);
    return bool(prediction_p);
}
//int count = 0;

void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
    //count++; 
    /*vector<int> input(32, 0); 

    for (int i = 31; i >= 0; --i) {
        input[i] = (PC & 1); 
        PC >>= 1;
    }*/   
    int prediction_p = perceptron.predict(PC);
    int prediction_t = GetPrediction_2bitsat_open(PC);
    if (prediction_p != prediction_t) {
        if (prediction_p == resolveDir) {
            num_of_competence++;
	    if (num_of_competence > 2) {
                num_of_competence = 2;
     	    }
        } else {
            num_of_competence--;
            if (num_of_competence < -2) {
                num_of_competence = -2;
            }   

	}
    }
    if (bool(prediction_p) != resolveDir) {perceptron.train(PC, resolveDir);} else {perceptron_correct++;}
    for (int i = 0 ; i < GHR_size; i++) {
        GHR[i] = GHR[i+1];
    }
    GHR[GHR_size-1] = ((int)resolveDir == 1);
    //UpdatePredictor_2bitsat_open(PC, resolveDir, prediction_t, UINT32 branchTarget);
    if (bool(prediction_t) == resolveDir) {twobit_correct++;}
    UpdatePredictor_2bitsat_open(PC, resolveDir, prediction_t, branchTarget);
    /*if (resolveDir != predDir) {
        history[h_count].key = PC;
	history[h_count].value = branchTarget;
	h_count++;
	h_count %= 3;
    }*/
    //if (count == 14796020) perceptron.saveWeightsToFile("weights.bin");
}
 	
