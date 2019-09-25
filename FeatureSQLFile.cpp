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







  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //                                                            helper functions                                                          //
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  // convert int of enum datatype to a struct with prefix and type 
  PrefixSQLTypePair enumToPrefix(const DataValue::DataType& dt)
  {
    // create label prefix according to type
    String prefix;
    String type;
    PrefixSQLTypePair pSTP;
    switch (dt)
    {
      case DataValue::STRING_VALUE:
        pSTP.prefix = "_S_";
        pSTP.sqltype = "TEXT";
        break;
      case DataValue::INT_VALUE:
        pSTP.prefix = "_I_";
        pSTP.sqltype = "INTEGER";
        break;
      case DataValue::DOUBLE_VALUE:
        pSTP.prefix = "_D_";
        pSTP.sqltype = "FLOAT";
        break;
      case DataValue::STRING_LIST:
        pSTP.prefix = "_SL_";
        pSTP.sqltype = "TEXT";
        break;
      case DataValue::INT_LIST:
        pSTP.prefix = "_IL_";
        pSTP.sqltype = "TEXT";
        break;
      case DataValue::DOUBLE_LIST:
        pSTP.prefix = "_DL_";
        pSTP.sqltype = "TEXT";
        break;
      case DataValue::EMPTY_VALUE:
        pSTP.prefix = "";
        pSTP.sqltype = "TEXT";
        break;
    }
    return pSTP;
  }

  
  String createTable(const String& table_name, const String& table_stmt)
  {
    String create_table_stmt = "CREATE TABLE " + table_name + " (" + table_stmt + ");";
    return create_table_stmt;
  }






  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //                                                store FeatureMap as SQL database                                                      //
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // fitted snippet of FeatureXMLFile::load
  void FeatureSQLFile::write(const std::string& out_fm, const FeatureMap& feature_map) const
  {
    String path="/home/mkf/Development/OpenMS/src/tests/class_tests/openms/data/";
    String filename = path.append(out_fm);
    
    // delete file if present
    File::remove(filename);

 
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                                        variable declaration                                                          //
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////   
    // test for subordinate entries to ensure correct database instantiation
    bool subordinate_table_exists = false;
    for (auto feature : feature_map)
    {
      for (Feature sub : feature.getSubordinates())
      {
        int table_test = static_cast<int64_t>(sub.getUniqueId() & ~(1ULL << 63));
        {
          std::cout << table_test;
          if (table_test != -1)
          {
            subordinate_table_exists = true;
            break;
          }
          else
          {
            break;
          }
        }
      }
    }

    if (subordinate_table_exists)
    {
      std::cout << "subs here" << std::endl;
    }
    else
    {
      std::cout << "subs missing" << std::endl;
    }


    
    std::vector<String> feature_elements = {"ID", "RT", "MZ", "Intensity", "Charge", "Quality"};
    std::vector<String> feature_elements_types = {"INTEGER", "REAL", "REAL", "REAL", "INTEGER", "REAL"};
    
    std::vector<String> subordinate_elements = {"ID", "REF_ID", "RT", "MZ", "Intensity", "Charge", "Quality"};
    std::vector<String> subordinate_elements_types = {"INTEGER", "INTEGER", "REAL", "REAL", "REAL", "INTEGER", "REAL"};

    std::vector<String> dataprocessing_elements = {"ID", "SOFTWARE", "DATE", "TIME", "ACTIONS"};
    std::vector<String> dataprocessing_elements_types = {"INTEGER" ,"TEXT" ,"TEXT" ,"TEXT" , "TEXT"};


    // read feature_map values and store as key value map
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

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// build header feature for sql table                                                                                                  //
    /// build subordinate header as sql table                                                                                               //
    /// build dataprocessing header as sql table                                                                                            //
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // prepare feature header, add (dynamic) part of user_parameter labels to header and header type vector 
    for (const String& key : common_keys)
    {
      // feature_elements vector with strings prefix (_TYPE_, _S_, _IL_, ...) and key  
      feature_elements.push_back(enumToPrefix(map_key2type[key]).prefix + key);
      // feature_elements vector with SQL TYPES 
      feature_elements_types.push_back(enumToPrefix(map_key2type[key]).sqltype);
    }


    // prepare subordinate header
    std::map<String, DataValue::DataType> subordinate_key2type;

    for (FeatureMap::const_iterator feature_it = feature_map.begin(); feature_it != feature_map.end(); ++feature_it)
    {
      for (std::vector<Feature>::const_iterator sub_it = feature_it->getSubordinates().begin(); sub_it != feature_it->getSubordinates().end(); ++sub_it)
      { 
        std::vector<String> keys;
        sub_it->getKeys(keys);
        for (const String& key : keys)
        {
          subordinate_key2type[key] = sub_it->getMetaValue(key).valueType();
        }
      }
    }

    for (const auto& key2type : subordinate_key2type)
    {
      // subordinate_elements vector with strings prefix (_TYPE_, _S_, _IL_, ...) and key  
      subordinate_elements.push_back(enumToPrefix(subordinate_key2type[key2type.first]).prefix + key2type.first);
      // subordinate_elements_type vector with SQL TYPES 
      subordinate_elements_types.push_back(enumToPrefix(subordinate_key2type[key2type.second]).sqltype);
    }

    // prepare dataprocessing header
    // TODO?



    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                                     create database with empty tables                                                //
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // construct SQL_labels for feature, subordinate and dataprocessing tables
    // create empty
    // 1. features table
    // vector sql_labels, concatenate element and respective SQL type
    std::vector<String> sql_labels_features = {};
    for (std::size_t idx = 0; idx != feature_elements.size(); ++idx)
    {
      sql_labels_features.push_back(feature_elements[idx] + " " + feature_elements_types[idx]);
    }
    // add PRIMARY KEY to first entry, assuming 1st column contains primary key
    sql_labels_features[0].append(" PRIMARY KEY");
    // add "NOT NULL" to all entries
    std::for_each(sql_labels_features.begin(), sql_labels_features.end(), [] (String &s) {s.append(" NOT NULL");});
    // create single string with delimiter ',' as TABLE header template
    String sql_stmt_features = ListUtils::concatenate(sql_labels_features, ",");


    // 2. subordinates table
    // vector sql_labels, concatenate element and respective SQL type
    std::vector<String> sql_labels_subordinates = {};
    for (std::size_t idx = 0; idx != subordinate_elements.size(); ++idx)
    {
      sql_labels_subordinates.push_back(subordinate_elements[idx] + " " + subordinate_elements_types[idx]);
    }
    // add PRIMARY KEY to first entry, assuming 1st column contains primary key
    sql_labels_subordinates[0].append(" PRIMARY KEY");
    // add "NOT NULL" to all entries
    std::for_each(sql_labels_subordinates.begin(), sql_labels_subordinates.end(), [] (String &s) {s.append(" NOT NULL");});
    // create single string with delimiter ',' as TABLE header template
    String sql_stmt_subordinates = ListUtils::concatenate(sql_labels_subordinates, ",");

    // 3. dataprocessing table
    // see manual entry below
    std::vector<String> sql_labels_dataprocessing = {};
    for (std::size_t idx = 0; idx != dataprocessing_elements.size(); ++idx)
    {
      sql_labels_dataprocessing.push_back(dataprocessing_elements[idx] + " " + dataprocessing_elements_types[idx]);
    }
    // add PRIMARY KEY to first entry, assuming 1st column contains primary key
    sql_labels_dataprocessing[0].append(" PRIMARY KEY");
    // add "NOT NULL" to all entries
    std::for_each(sql_labels_dataprocessing.begin(), sql_labels_dataprocessing.end(), [] (String &s) {s.append(" NOT NULL");});
    // create single string with delimiter ',' as TABLE header template
    String sql_stmt_dataprocessing = ListUtils::concatenate(sql_labels_dataprocessing, ",");

    
    // create empty SQL table stmt
    // 1. features
    const String features_table_stmt = createTable("FEATURES", sql_stmt_features); 
    // 2. subordinates
    const String subordinates_table_stmt = createTable("SUBORDINATES", sql_stmt_subordinates); 
    // 3. dataprocessing
    const String dataprocessing_table_stmt = createTable("DATAPROCESSING", sql_stmt_dataprocessing); 



    String create_sql = \
      features_table_stmt + \
      
      subordinates_table_stmt + \
      
      dataprocessing_table_stmt \
      
      // closing statement semicolon
      ;    


    // create SQL database, create empty SQL tables  see (https://github.com/OpenMS/OpenMS/blob/develop/src/openms/source/FORMAT/OSWFile.cpp)
    // Open connection to database
    SqliteConnector conn(filename);
    conn.executeStatement(create_sql);






    // break into functions?
    // turn into line feed function for SQL database input step statement
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                                     store FeatureMap data in tables                                                  //
    //                                                     1. features                                                                      //                                    
    //                                                     2. subordinates                                                                  //
    //                                                     3. dataprocessing                                                                //
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Iteration over FeatureMap
    // turn into line feed function for SQL database input step statement
    /// 1. insert data of features table
    const String feature_elements_sql_stmt = ListUtils::concatenate(feature_elements, ","); 
    for (auto it = feature_map.begin(); it != feature_map.end(); ++it)
    {
      String line_stmt;
      std::vector<String> line;

      line.push_back(static_cast<int64_t>(it->getUniqueId() & ~(1ULL << 63)));
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
      //store in features table
      conn.executeStatement(line_stmt);
    }


    /// 2. insert data of subordinates table
    String line_stmt;
    const String subordinate_elements_sql_stmt = ListUtils::concatenate(subordinate_elements, ","); 
    //for (auto feature : feature_map)
    for (const Feature& feature : feature_map)
    {
      std::vector<String> line;
      for (const Feature& sub : feature.getSubordinates())
      {          
        line.push_back(static_cast<int64_t>(sub.getUniqueId() & ~(1ULL << 63)));
        line.push_back(static_cast<int64_t>(feature.getUniqueId() & ~(1ULL << 63)));
        line.push_back(sub.getRT());
        line.push_back(sub.getMZ());
        line.push_back(sub.getIntensity());
        line.push_back(sub.getCharge());
        line.push_back(sub.getOverallQuality());
        for (const std::pair<String, DataValue::DataType> k2t : subordinate_key2type)
        {
          const String& key = k2t.first;
          if (sub.metaValueExists(key))
          {
            // alle subordinates durchlaufen?, schauen ob metavalue aller schluessel vorhanden
            // falls ja, hole wert und addiere zum 
            // line statement dazu
            String s = sub.getMetaValue(k2t.first);
            if (k2t.second == DataValue::STRING_VALUE
              || k2t.second == DataValue::STRING_LIST)
            {
              s = s.substitute("'", "''"); // SQL escape single quote in strings  
            }

            if (k2t.second == DataValue::STRING_VALUE
              || k2t.second == DataValue::STRING_LIST
              || k2t.second == DataValue::INT_LIST
              || k2t.second == DataValue::DOUBLE_LIST)
            {
              s = "'" + s + "'"; // quote around SQL strings (and list types)
            }
            line.push_back(s);
          }
        }
        line_stmt =  "INSERT INTO SUBORDINATES (" + subordinate_elements_sql_stmt + ") VALUES (";
        line_stmt += ListUtils::concatenate(line, ",");
        line_stmt += ");";
        //store in subordinates table
        conn.executeStatement(line_stmt);
        //clear vector line
        line.clear();
      }
    }


    /// 3. insert data of dataprocessing table
    const String dataprocessing_elements_sql_stmt = ListUtils::concatenate(dataprocessing_elements, ","); 
    std::cout << dataprocessing_elements_sql_stmt << std::endl;
    std::cout << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" << std::endl;
    std::vector<String> dataproc_elems = {};
    dataproc_elems.push_back(static_cast<int64_t>(feature_map.getUniqueId() & ~(1ULL << 63)));
    const std::vector<DataProcessing> dataprocessing = feature_map.getDataProcessing();
  
    for (auto datap : dataprocessing)
    {
      dataproc_elems.push_back(datap.getSoftware().getName());
      dataproc_elems.push_back(datap.getCompletionTime().getDate());
      dataproc_elems.push_back(datap.getCompletionTime().getTime());

      const std::set<DataProcessing::ProcessingAction>& proc_ac = datap.getProcessingActions();

      String processing_action_names;
      for (const DataProcessing::ProcessingAction& a : proc_ac)
      {
        const String& action = DataProcessing::NamesOfProcessingAction[int(a)];
        processing_action_names += action;
      }
      dataproc_elems.push_back(processing_action_names);
    }

    for (std::size_t idx = 0; idx != dataprocessing_elements.size(); ++idx)
    {
      //if (dataprocessing_elements_types[elem] == "TEXT")
      {
        std::cout << dataprocessing_elements[idx] << std::endl;
        std::cout << dataprocessing_elements_types[idx] << std::endl;
        if (dataprocessing_elements_types[idx] == "TEXT")
        {
          dataproc_elems[idx] = "'" + dataproc_elems[idx] + "'"; 
        }
      }
    }

    
    std::cout << ListUtils::concatenate(dataproc_elems, ",");
    
    line_stmt =  "INSERT INTO DATAPROCESSING (" + dataprocessing_elements_sql_stmt + ") VALUES (";
    line_stmt += ListUtils::concatenate(dataproc_elems, ",");
    line_stmt += ");";
    //store in dataprocessing table
    conn.executeStatement(line_stmt);
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

//std::cout << key << "\t" << map_key2type[key] << std::endl;
//  std::cout << key << "\t" << enumToPrefix(map_key2type[key]).prefix << std::endl;


    //std::cout << feature_elements << std::endl;


    //stringstream header = "";
    //for_each(feature_elements.begin(), feature_elements.end(), [&header] (const std::string& feature_element) { cat(header, feature_element)});
    



    //for (const auto& key2type : subordinate_key2type)
    for ( auto& key2type : subordinate_key2type)
    {
      std::cout << key2type.first << std::endl;
      std::cout << key2type.second << std::endl;
    }



    for (const Feature& feature : feature_map)
    {
      for (const Feature& sub : feature.getSubordinates())
      { 
        // for all user param keys present in subordinates
        for (const std::pair<String, DataValue::DataType> k2t : subordinate_key2type)
        {
          const String& key = k2t.first;
          if (sub.metaValueExists(key))
          {
            // alle subordinates durchlaufen?, schauen ob metavalue aller schluessel vorhanden
            // falls ja, hole wert und addiere zum 
            // line statement dazu

          }
          else 
          {
            // fuege 
          }
          
        }
      }
    }

    String line_stmt_subordinate;
    std::vector<String> line_sub;

    // prepare subordinate header
    for (FeatureMap::const_iterator feature_it = feature_map.begin(); feature_it != feature_map.end(); ++feature_it)
    //for (auto feature : feature_map)
    {


      for (std::vector<Feature>::const_iterator sub_it = feature_it->getSubordinates().begin(); sub_it != feature_it->getSubordinates().end(); ++sub_it)
      //  const std::vector<Feature>& feature.getSubordinates(
      { 

        std::vector<String> keys = {};
        sub_it->getKeys(keys);
        for (auto userparam : keys)
        {
          line_sub.push_back(static_cast<int64_t>(sub_it->getUniqueId() & ~(1ULL << 63)));
          line_sub.push_back(static_cast<int64_t>(feature_it->getUniqueId() & ~(1ULL << 63)));
          line_sub.push_back(sub_it->getRT()() << std::endl;
          line_sub.push_back(sub_it->getMetaValue(userparam) << std::endl;
          line_sub.push_back(sub_it->getIntensity());
          line_sub.push_back(sub_it->getCharge());
          line_sub.push_back(sub_it->getOverallQuality());
        }
        //std::cout << sub_it->getMZ() << std::endl;
      }
    }
    line_stmt_subordinate += ListUtils::concatenate(line_sub, ",");

    std::cout << line_stmt_subordinate << std::endl;












    std::cout << "-----------------------------------------" << std::endl;
    std::cout << line_stmt << std::endl;
    std::cout << "-----------------------------------------" << std::endl;
    std::cout << subordinate_elements_sql_stmt << std::endl;



*/