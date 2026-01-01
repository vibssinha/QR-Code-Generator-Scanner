#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mongoc/mongoc.h>
#include <zbar.h>
#include <opencv2/opencv.hpp>
#include <bson/bson.h>

//Add your own Mongo URI
#define MONGO_URI ""
//Add your own database name
#define DATABASE_NAME ""
//Add your own collection name
#define COLLECTION_NAME ""

using namespace cv;
using namespace zbar;

//Add more to struct if you have more data
typedef struct {
    char name[100];
    int code;
    char date[20];
    int ticket_number;
} TicketData;

// Parse JSON from QR code
int parse_qr_json(const char* json_str, TicketData* ticket) {
    bson_error_t error;
    bson_t* doc = bson_new_from_json((const uint8_t*)json_str, -1, &error);
    
    if (!doc) {
        printf("Failed to parse JSON: %s\n", error.message);
        return 0;
    }
    
    bson_iter_t iter;
    
    if (bson_iter_init_find(&iter, doc, "Name")) {
        strncpy(ticket->name, bson_iter_utf8(&iter, NULL), sizeof(ticket->name) - 1);
    }
    
    if (bson_iter_init_find(&iter, doc, "Code")) {
        ticket->code = bson_iter_int32(&iter);
    }
    
    if (bson_iter_init_find(&iter, doc, "Date")) {
        strncpy(ticket->date, bson_iter_utf8(&iter, NULL), sizeof(ticket->date) - 1);
    }
    
    if (bson_iter_init_find(&iter, doc, "TicketNumber")) {
        ticket->ticket_number = bson_iter_int32(&iter);
    }
  //add more or take away if statements if you have different amounts of data
    
    bson_destroy(doc);
    return 1;
}

// Validate ticket against MongoDB
int validate_ticket(mongoc_collection_t* collection, TicketData* ticket) {
    bson_t* query = bson_new();
    BSON_APPEND_INT32(query, "Code", ticket->code);
    
    const bson_t* doc;
    mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);
    
    int found = 0;
    if (mongoc_cursor_next(cursor, &doc)) {
        bson_iter_t iter;
        
        // Validate all fields match
        int code_match = 0, name_match = 0, date_match = 0, ticket_match = 0;
        
        if (bson_iter_init_find(&iter, doc, "Code")) {
            code_match = (bson_iter_int32(&iter) == ticket->code);
        }
        
        if (bson_iter_init_find(&iter, doc, "Name")) {
            const char* db_name = bson_iter_utf8(&iter, NULL);
            name_match = (strcmp(db_name, ticket->name) == 0);
        }
        
        if (bson_iter_init_find(&iter, doc, "Date")) {
            const char* db_date = bson_iter_utf8(&iter, NULL);
            date_match = (strcmp(db_date, ticket->date) == 0);
        }
        
        if (bson_iter_init_find(&iter, doc, "TicketNumber")) {
            ticket_match = (bson_iter_int32(&iter) == ticket->ticket_number);
        }
      //add more or take away some if you have more data points
        
        found = (code_match && name_match && date_match && ticket_match);
        
        if (found) {
            printf("\n✓ TICKET VALIDATED\n");
            printf("Name: %s\n", ticket->name);
            printf("Code: %d\n", ticket->code);
            printf("Date: %s\n", ticket->date);
            printf("Ticket Number: %d\n", ticket->ticket_number);
        }
        else {
            printf("\n✗ VALIDATION FAILED - Data mismatch\n");
        }
    } 
    else {
        printf("\n✗ VALIDATION FAILED - Ticket not found in database\n");
    }
    
    mongoc_cursor_destroy(cursor);
    bson_destroy(query);
    
    return found;
}

// Scan QR code from camera
char* scan_qr_code(VideoCapture& cap) {
    ImageScanner scanner;
    scanner.set_config(ZBAR_NONE, ZBAR_CFG_ENABLE, 1);
    
    Mat frame, gray;
    
    printf("Scanning for QR code...\n");
    
    while (true) {
        cap >> frame;
        
        if (frame.empty()) {
            printf("Failed to capture frame\n");
            continue;
        }
        
        cvtColor(frame, gray, COLOR_BGR2GRAY);
        
        int width = gray.cols;
        int height = gray.rows;
        uchar* raw = gray.data;
        
        Image image(width, height, "Y800", raw, width * height);
        
        int n = scanner.scan(image);
        
        if (n > 0) {
            for (Image::SymbolIterator symbol = image.symbol_begin(); 
                 symbol != image.symbol_end(); ++symbol) {
                
                if (symbol->get_type() == ZBAR_QRCODE) {
                   std:: string data = symbol->get_data();
                    char* result = (char*)malloc(data.length() + 1);
                    strcpy(result, data.c_str());
                    
                    printf("QR Code detected!\n");
                    return result;
                }
            }
        }
        
        imshow("QR Scanner", frame);
        
        if (waitKey(30) == 27) { // ESC key
            break;
        }
    }
    
    return NULL;
}

int main() {
    // Initialize MongoDB
    mongoc_init();
    
    mongoc_client_t* client = mongoc_client_new(MONGO_URI);
    if (!client) {
        fprintf(stderr, "Failed to connect to MongoDB\n");
        return 1;
    }
    
    mongoc_database_t* database = mongoc_client_get_database(client, DATABASE_NAME);
    mongoc_collection_t* collection = mongoc_client_get_collection(client, DATABASE_NAME, COLLECTION_NAME);
    
    printf("Connected to MongoDB Atlas\n");
    
    // Initialize camera
    VideoCapture cap(0); // 0 for default camera
    
    if (!cap.isOpened()) {
        fprintf(stderr, "Failed to open camera\n");
        mongoc_collection_destroy(collection);
        mongoc_database_destroy(database);
        mongoc_client_destroy(client);
        mongoc_cleanup();
        return 1;
    }
    
    printf("Camera initialized\n");
    printf("Press ESC to exit\n\n");
    
    // Main scanning loop
    while (true) {
        char* qr_data = scan_qr_code(cap);
        
        if (qr_data != NULL) {
            TicketData ticket = {0};
            
            if (parse_qr_json(qr_data, &ticket)) {
                validate_ticket(collection, &ticket);
            }
            
            free(qr_data);
            
            printf("\nWaiting 3 seconds before next scan...\n\n");
            sleep(3);
            printf("Scan again\n");
        }
    }
    
    // Cleanup
    cap.release();
    destroyAllWindows();
    mongoc_collection_destroy(collection);
    mongoc_database_destroy(database);
    mongoc_client_destroy(client);
    mongoc_cleanup();
    
    return 0;
}
