// --------------------------------------------------------------------------
//           OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2018.
//
// This software is released under a three-clause BSD license:
//  * Redistributions of source code must retain the above copyright
//  notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//  notice, this list of conditions and the following disclaimer in the
//  documentation and/or other materials provided with the distribution.
//  * Neither the name of any author or any participating institution
//  may be used to endorse or promote products derived from this software
//  without specific prior written permission.
// For a full list of authors, refer to the file AUTHORS.
// --------------------------------------------------------------------------
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ANY OF THE AUTHORS OR THE CONTRIBUTING
// INSTITUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// --------------------------------------------------------------------------
// $Maintainer: Matthias Fuchs $
// $Authors: Matthias Fuchs $
// --------------------------------------------------------------------------

#include <OpenMS/KERNEL/FeatureMap.h>
#include <OpenMS/FORMAT/SqliteConnector.h>
#include <OpenMS/FORMAT/FeatureSQLFile.h>

#include <OpenMS/METADATA/DataProcessing.h>
#include <OpenMS/METADATA/MetaInfoInterface.h>
#include <OpenMS/METADATA/MetaInfoInterfaceUtils.h>
#include <OpenMS/DATASTRUCTURES/ListUtils.h>

#include <OpenMS/CONCEPT/UniqueIdInterface.h>
#include <OpenMS/DATASTRUCTURES/DateTime.h>
#include <OpenMS/DATASTRUCTURES/Param.h>
#include <OpenMS/DATASTRUCTURES/String.h>

#include <sqlite3.h>
#include <sstream>
#include <map>
#include <boost/algorithm/string/join.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <vector>

#include <OpenMS/FORMAT/FileHandler.h>
#include <OpenMS/SYSTEM/File.h>

// #include <Boost>


namespace OpenMS
{
  // helper functions:  
  //std::string enumToPrefix(const DataValue::DataType& dt)
  // translocate strict to FeatureSQLFile.h

  // convert int of enum datatype to  
  PrefixSQLTypePair enumToPrefix(const int dt)
  {
    // create label prefix according to type
    std::string prefix = "";
    std::string type = "";
    PrefixSQLTypePair pSTP;
    switch(dt){
      case 0:
        pSTP.prefix = "_S_";
        pSTP.sqltype = "TEXT";
        break;
      case 1:
        pSTP.prefix = "_I_";
        pSTP.sqltype = "INTEGER";
        break;
      case 2:
        pSTP.prefix = "_D_";
        pSTP.sqltype = "FLOAT";
        break;
      case 3:
        pSTP.prefix = "_SL_";
        pSTP.sqltype = "TEXT";
        break;
      case 4:
        pSTP.prefix = "_IL_";
        pSTP.sqltype = "TEXT";
        break;
      case 5:
        pSTP.prefix = "_DL_";
        pSTP.sqltype = "TEXT";
        break;
      case 6:
        pSTP.prefix = "";
        pSTP.sqltype = "TEXT";
        break;
    }
    return pSTP;
  }

  // store FeatureMap as SQL database
  // fitted snippet of FeatureXMLFile::load
  void FeatureSQLFile::write(const std::string& out_fm, const FeatureMap& feature_map) const
  {
    std::string path="/home/mkf/Development/OpenMS/src/tests/class_tests/openms/data/";
    std::string filename = path.append(out_fm);

    std::vector<std::string> feature_elements = {"ID", "RT", "MZ", "Intensity", "Charge", "Quality"};
    std::vector<std::string> feature_elements_types = {"INTEGER", "REAL", "REAL", "REAL", "INTEGER", "REAL"};
    
    // read feature_map values and insert into map
    std::set<String> common_keys = MetaInfoInterfaceUtils::findCommonMetaKeys<FeatureMap, std::set<String> >(feature_map.begin(), feature_map.end(), 0.0);

    std::map<String, DataValue::DataType> map_key2type;

    for (auto feature : feature_map)
    {
      for (const String& key : common_keys)
      {
        if (feature.metaValueExists(key))
        {
          const DataValue::DataType& dt = feature.getMetaValue(key).valueType();
          map_key2type[key] = dt;
        }
      }
    }

    for (const String& key : common_keys)
    {
      //std::cout << key << "\t" << map_key2type[key] << std::endl;
      //  std::cout << key << "\t" << enumToPrefix(map_key2type[key]).prefix << std::endl;
      // build feature_elements vector with prefixes and keys
      feature_elements.push_back(enumToPrefix(map_key2type[key]).prefix + key);
      //feature_elements_helper.push_back(enumToPrefix(map_key2type[key]).prefix += key);
      // build feature_elements type vector for SQL stmt
      feature_elements_types.push_back(enumToPrefix(map_key2type[key]).sqltype);
    }

    //std::cout << feature_elements << std::endl;

    //stringstream header = "";
    //for_each(feature_elements.begin(), feature_elements.end(), [&header] (const std::string& feature_element) { cat(header, feature_element)});
    
    // create new vector sql_stmt
    // add type 
    std::vector<std::string> sql_labels = {};
    for (std::size_t idx = 0; idx != feature_elements.size(); ++idx)
    {
      sql_labels.push_back(feature_elements[idx] + " " + feature_elements_types[idx]);
    }

    // add PRIMARY KEY to first entry
    sql_labels[0].append(" PRIMARY KEY");

    // add "NOT NULL" to each entry
    std::for_each(sql_labels.begin(), sql_labels.end(), [] (std::string &s) {s.append(" NOT NULL");});
    
    // generate single string with delimiter
    std::string sql_stmt = ListUtils::concatenate(sql_labels, ",");
    
    std::cout << sql_stmt << std::endl;

    /* table features: sql statement creation finished */       

    // Write sqlite database
    //sqlite3_stmt * cntstmt;
    //sqlite3_stmt * stmt;
    //std::string select_sql;
    
    // delete file if present
    File::remove(filename);
    
    //https://github.com/OpenMS/OpenMS/blob/develop/src/openms/source/FORMAT/OSWFile.cpp
    // Open database
    SqliteConnector conn(filename);
    //sqlite3 *db;
 
    // Create SQL structure
    std::string create_sql =
    // data_processing table
      "CREATE TABLE DATA_PROCESSING(" \
      "ID INT PRIMARY KEY NOT NULL," \
      "SOFTWARE TEXT NOT NULL," \
      "DATA TEXT NOT NULL," \
      "TIME TEXT NOT NULL," \
      "ACTIONS TEXT NOT NULL);" \

    // features table
      "CREATE TABLE FEATURES(" \
      + sql_stmt + \
      ");"

    // subordinates table
      "CREATE TABLE SUBORDINATES(" \
      "ID INT PRIMARY KEY NOT NULL," \
      "FEATURE_REF TEXT NOT NULL);";

    
    // Execute SQL create statement
    conn.executeStatement(create_sql);

    // populate SQL database with map entries
    
    // export as SQL database
    //    std::vector<std::string> feature_elements = {"ID", "RT", "MZ", "Intensity", "Charge", "Quality"};



    // Iteration over FeatureMap
    // turn into line feed function for SQL database input step statement
    //std::string
    //std::string

    //int counter = 1;
    const String feature_elements_sql_stmt = ListUtils::concatenate(feature_elements, ","); 
    for (auto it = feature_map.begin(); it != feature_map.end(); ++it)
    {
      
      String line_stmt;
      std::vector<String> line;

      //line.push_back(it->getUniqueId());
      line.push_back(static_cast<int64_t>(it->getUniqueId() & ~(1ULL << 63)));
      //line.push_back(counter);
      //counter++;
      line.push_back(it->getRT());
      line.push_back(it->getMZ());
      line.push_back(it->getIntensity());
      line.push_back(it->getCharge());
      line.push_back(it->getOverallQuality());
      for (const String& key : common_keys)
      {
        String s = it->getMetaValue(key);
        if (map_key2type[key] == DataValue::STRING_VALUE
          || map_key2type[key] == DataValue::STRING_LIST)
        {
          s = s.substitute("'", "''"); // SQL escape single quote in strings  
        }

        if (map_key2type[key] == DataValue::STRING_VALUE
          || map_key2type[key] == DataValue::STRING_LIST
          || map_key2type[key] == DataValue::INT_LIST
          || map_key2type[key] == DataValue::DOUBLE_LIST)
        {
          s = "'" + s + "'"; // quote around SQL strings (and list types)
        }

        line.push_back(s);
      }
      line_stmt =  "INSERT INTO FEATURES (" + feature_elements_sql_stmt + ") VALUES (";
      line_stmt += ListUtils::concatenate(line, ",");
      line_stmt += ");";

      std::cout << "-----------------------------------------" << std::endl;
      std::cout << line_stmt << std::endl;
      std::cout << std::endl;

      conn.executeStatement(line_stmt);
    }


    //std::string line_stmt = ListUtils::concatenate(line, ",");
    
    std::cout << feature_elements_sql_stmt << std::endl;
    //std::cout <<  << std::endl;

    /*

      for (const String& key : common_keys)
      {
        line.push_back(it->getMetaValue(key));
      }
      line_smt
      create_sql =  "INSERT INTO FEATURES VALUES(" + ListUtils::concatenate(line, ",") + ");"); 
      conn.executeStatement(create_sql);
    }

    std::cout << line_stmt << std::endl;

    
    
    */

    /*
    for (auto feature : feature_map)
    {
      for (const String& key : common_keys)
      {
        if (feature.metaValueExists(key))
        {
          const DataValue::DataType& dt = feature.getMetaValue(key).valueType();
          map_key2type[key] = dt;
        }
      }
    }
    */

  }
} // end of FeatureSQLFile::write
























  /*
  // read SQL database and store as FeatureMap
  // based on TransitionPQPFile::readPQPInput_

  FeatureMap FeatureSQLFile::read(const std::string& filename) const
  {
    sqlite3 *db;
    sqlite3_stmt * cntstmt;
    sqlite3_stmt * stmt;
    std::string select_sql;
    SqliteConnector conn(filename);
    
    // connect database and read tables
    db = conn.getDB();

    //create empty FeatureMap object
    FeatureMap feature_map;

    // get table entries if existent
    bool data_processing_exists = SqliteConnector::tableExists(db, "DATA_PROCESSING");
    if (data_processing_exists)
    {    
      // Execute SQL select statement
      conn.executePreparedStatement(&stmt, select_sql);
      sqlite3_step( stmt );

      while (sqlite3_column_type( stmt, 0 ) != SQLITE_NULL)
      {
        setProgress(progress++);
        Feature current_feature;

        Sql::extractValue<double>(&current_feature.setUniqueId(), stmt, 0);
        Sql::extractValue<double>(&mytransition.library_intensity, stmt, 5);
        Sql::extractValue<std::string>(&mytransition.group_id, stmt, 6);

      }

    }

    bool features_exists = SqliteConnector::tableExists(db, "FEATURES");
    if (features_exists)
    {
    }

    bool subordinates_exists = SqliteConnector::tableExists(db, "SUBORDINATES");
    if (subordinates_exists)
    {
      // if column FEATURE_REF exists  
      bool feature_ref = SqliteConnector::columnExists(db, "SUBORDINATES", "FEATURE_REF");

    }


    // import features and subordinates

    // save data as featureMap
  }
  */
//} // namespace OpenMS




/*
// https://www.geeksforgeeks.org/sql-using-c-c-and-sqlite/
//database access

{ 
    sqlite3* DB; 
    int exit = 0; 
    exit = sqlite3_open("example.db", &DB); 
  
    if (exit) { 
        std::cerr << "Error open DB " << sqlite3_errmsg(DB) << std::endl; 
        return (-1); 
    } 
    else
        std::cout << "Opened Database Successfully!" << std::endl; 
    sqlite3_close(DB); 
    return (0); 
} 

// create 3 databases depending upon number of FeatureMap depth
// 1. dataprocessing
// 2. features
// 3. subordinates
{ 
    sqlite3* DB; 
    std::string sql = "CREATE TABLE dataprocessing("
                      "ID INT PRIMARY KEY     NOT NULL, "
                      "NAME           TEXT    NOT NULL, "
                      "SURNAME        TEXT    NOT NULL, "
                      "AGE            INT     NOT NULL, "
                      "ADDRESS        CHAR(50), "
                      "SALARY         REAL );"; 
    
    int exit = 0; 
    exit = sqlite3_open("example.db", &DB); 
    char* messaggeError; 
    exit = sqlite3_exec(DB, sql.c_str(), NULL, 0, &messaggeError); 
  
    if (exit != SQLITE_OK) { 
        std::cerr << "Error Create Table" << std::endl; 
        sqlite3_free(messaggeError); 
    } 
    else
        std::cout << "Table created Successfully" << std::endl; 
    sqlite3_close(DB); 
    return (0); 
} 


// insert and delete

static int callback(void* data, int argc, char** argv, char** azColName) 
{ 
    int i; 
    fprintf(stderr, "%s: ", (const char*)data); 
  
    for (i = 0; i < argc; i++) { 
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL"); 
    } 
  
    printf("\n"); 
    return 0; 
} 
  
int main(int argc, char** argv) 
{ 
    sqlite3* DB; 
    char* messaggeError; 
    int exit = sqlite3_open("example.db", &DB); 
    string query = "SELECT * FROM PERSON;"; 
  
    cout << "STATE OF TABLE BEFORE INSERT" << endl; 
  
    sqlite3_exec(DB, query.c_str(), callback, NULL, NULL); 
  
    string sql("INSERT INTO PERSON VALUES(1, 'STEVE', 'GATES', 30, 'PALO ALTO', 1000.0);"); 
  
    exit = sqlite3_exec(DB, sql.c_str(), NULL, 0, &messaggeError); 
    if (exit != SQLITE_OK) { 
        std::cerr << "Error Insert" << std::endl; 
        sqlite3_free(messaggeError); 
    } 
    else
        std::cout << "Records created Successfully!" << std::endl; 
  
    cout << "STATE OF TABLE AFTER INSERT" << endl; 
  
    sqlite3_exec(DB, query.c_str(), callback, NULL, NULL); 
  
    sql = "DELETE FROM PERSON WHERE ID = 2;"; 
    exit = sqlite3_exec(DB, sql.c_str(), NULL, 0, &messaggeError); 
    if (exit != SQLITE_OK) { 
        std::cerr << "Error DELETE" << std::endl; 
        sqlite3_free(messaggeError); 
    } 
    else
        std::cout << "Record deleted Successfully!" << std::endl; 
  
    cout << "STATE OF TABLE AFTER DELETE OF ELEMENT" << endl; 
    sqlite3_exec(DB, query.c_str(), callback, NULL, NULL); 
  
    sqlite3_close(DB); 
    return (0); 
} 



// select operation
int main(int argc, char** argv) 
{ 
    sqlite3* DB; 
    int exit = 0; 
    exit = sqlite3_open("example.db", &DB); 
    string data("CALLBACK FUNCTION"); 
  
    string sql("SELECT * FROM PERSON;"); 
    if (exit) { 
        std::cerr << "Error open DB " << sqlite3_errmsg(DB) << std::endl; 
        return (-1); 
    } 
    else
        std::cout << "Opened Database Successfully!" << std::endl; 
  
    int rc = sqlite3_exec(DB, sql.c_str(), callback, (void*)data.c_str(), NULL); 
  
    if (rc != SQLITE_OK) 
        cerr << "Error SELECT" << endl; 
    else { 
        cout << "Operation OK!" << endl; 
    } 
  
    sqlite3_close(DB); 
    return (0); 
} 
*/


/* SNIPPETS BLOCK

    //std::cout << boost::algorithm::join(feature_elements_types, ", ") << std::endl;

*/