import qrcode
from pymongo import MongoClient
import json
from datetime import datetime

class QRCodeGenerator:
    #Initialize connection to mongo db database
    def __init__(self, mongo_uri, database_name, collection_name):
        self.client = MongoClient(mongo_uri)
        self.db = self.client[database_name]
        self.collection = self.db[collection_name]
    
    def generate_qr_from_code(self, code, output_filename=None):
        # Fetch document from MongoDB. Change this code to something else if you want to identify data by something else
        document = self.collection.find_one({"Code": code})
        
        #Return if document is not there
        if not document:
            print(f"No document found with Code: {code}")
            return None
        
        # Remove the id field
        document.pop('_id', None) 
        
        # Jsonify the document
        qr_data = json.dumps(document)
        
        # Create QR code. Change to whatever you like
        qr = qrcode.QRCode(
            version=3,
            error_correction=qrcode.constants.ERROR_CORRECT_M,
            box_size=15,
            border=4,
        )
        qr.add_data(qr_data)

        # Generate image. Chnage the colors to whatever you like
        img = qr.make_image(fill_color="black", back_color="white")
        
        # Save to file
        if output_filename is None:
            output_filename = f"qr_{code}.png"
        
        img.save(output_filename)
        print(f"QR code generated: {output_filename}")
        print("QR code succesfully generated")
        
        return output_filename
    
    
    def close(self):
        self.client.close()


# Example usage
if __name__ == "__main__":
    # Configuration
    MONGO_URI = ""  # Update with your MongoDB URI
    DATABASE_NAME = "" #Update with Database name
    COLLECTION_NAME = "" #Update with Collection name
    
    # Initialize generator
    generator = QRCodeGenerator(MONGO_URI, DATABASE_NAME, COLLECTION_NAME)

    #Example of generating code
    generator.generate_qr_from_code(1234)
    
    
    # Close connection
    generator.close()
