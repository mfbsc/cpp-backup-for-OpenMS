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


#include <OpenMS/CONCEPT/Exception.h>
#include <OpenMS/CONCEPT/UniqueIdInterface.h>

#include <OpenMS/DATASTRUCTURES/DateTime.h>
#include <OpenMS/DATASTRUCTURES/DataValue.h>
#include <OpenMS/DATASTRUCTURES/ListUtils.h>
#include <OpenMS/DATASTRUCTURES/Param.h>
#include <OpenMS/DATASTRUCTURES/String.h>

#include <OpenMS/FORMAT/FeatureSQLFile.h>
#include <OpenMS/FORMAT/FileHandler.h>
#include <OpenMS/FORMAT/SqliteConnector.h>

#include <OpenMS/KERNEL/FeatureMap.h>
#include <OpenMS/KERNEL/StandardTypes.h>

#include <OpenMS/METADATA/DataProcessing.h>
#include <OpenMS/METADATA/MetaInfoInterface.h>
#include <OpenMS/METADATA/MetaInfoInterfaceUtils.h>

#include <OpenMS/SYSTEM/File.h>

#include <unordered_set>
#include <sqlite3.h>
#include <sstream>
#include <map>
#include <boost/algorithm/string/join.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <vector>
#include <boost/algorithm/string.hpp>

// #include <Boost>


namespace OpenMS
{ 

  namespace Sql = Internal::SqliteHelper;



  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //                                                            helper functions                                                          //
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  // write part
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


  // get datatype
  //if ((dt == DataValue::STRING_VALUE) || (dt == DataValue::STRING_LIST))
  DataValue::DataType getColumnDatatype(String label)
  { 
    DataValue::DataType type;
    if (label.hasPrefix("_S_"))
    {
      type = DataValue::STRING_VALUE;
    } else
    if (label.hasPrefix("_I_"))
    {
      type = DataValue::INT_VALUE;
    } else
    if (label.hasPrefix("_D_"))
    {
      type = DataValue::DOUBLE_VALUE;      
    } else
    if (label.hasPrefix("_SL_"))
    {
      type = DataValue::STRING_LIST;
    } else
    if (label.hasPrefix("_IL_"))
    {
      type = DataValue::INT_LIST;
    } else
    if (label.hasPrefix("_DL_"))
    {
      type = DataValue::DOUBLE_LIST;
    } else 
    {
      type = DataValue::EMPTY_VALUE;
      /*
      if (!label.hasPrefix("_"))
      {
        type = 6;
      }
      */
    }
    return type;
  }



  // get userparameter
  String getColumnName(String label)
  { 
    String userparameter;
    if (label.hasPrefix("_S_"))
    {
      userparameter = label.substr(3);
    } else
    if (label.hasPrefix("_I_"))
    {
      userparameter = label.substr(3);
    } else
    if (label.hasPrefix("_D_"))
    {
      userparameter = label.substr(3);
    } else
    if (label.hasPrefix("_SL_"))
    {
      userparameter = label.substr(4);
    } else
    if (label.hasPrefix("_IL_"))
    {
      userparameter = label.substr(4);
    } else
    if (label.hasPrefix("_DL_"))
    {
      userparameter = label.substr(4);
    } else 
    {
      userparameter = label;
      /*
      if (!label.hasPrefix("_"))
      {
        datatype = 6;
      }
      */
    }
    return userparameter;
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
    
    /*
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
    */




    // declare table entries
    std::vector<String> feature_elements = {"ID", "RT", "MZ", "Intensity", "Charge", "Quality"};
    std::vector<String> feature_elements_types = {"INTEGER", "REAL", "REAL", "REAL", "INTEGER", "REAL"};
    
    std::vector<String> subordinate_elements = {"ID", "REF_ID", "RT", "MZ", "Intensity", "Charge", "Quality"};
    std::vector<String> subordinate_elements_types = {"INTEGER", "INTEGER", "REAL", "REAL", "REAL", "INTEGER", "REAL"};

    std::vector<String> dataprocessing_elements = {"ID", "SOFTWARE", "DATA", "TIME", "ACTIONS"};
    std::vector<String> dataprocessing_elements_types = {"INTEGER" ,"TEXT" ,"TEXT" ,"TEXT" , "TEXT"};



    // read feature_map userparameter values and store as key value map
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



    // read feature_map dataprocessing userparameter values and store as key value map
    std::vector<String> dataproc_keys;
    const std::vector<DataProcessing> dataprocessing_userparams = feature_map.getDataProcessing();
    std::map<String, DataValue::DataType> dataproc_map_key2type;

    for (auto datap : dataprocessing_userparams)
    {
      datap.getKeys(dataproc_keys);
      for (auto key : dataproc_keys)
      {
        const DataValue::DataType& dt = datap.getMetaValue(key).valueType(); 
        dataproc_map_key2type[key] = dt;

        /*
        const DataValue::DataType& dt = datap.getMetaValue(key).valueType(); 
        if ((dt == DataValue::STRING_VALUE) || (dt == DataValue::STRING_LIST))
        {
          dataproc_map_key2type[key] = dataproc_map_key2type["\"" + key + "\""];
        }
        dataproc_map_key2type.erase(key);
        */

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
    for (const auto& key2type : dataproc_map_key2type)
    {
      // dataprocessing vector with strings prefix (_TYPE_, _S_, _IL_, ...) and key  
      dataprocessing_elements.push_back(enumToPrefix(dataproc_map_key2type[key2type.first]).prefix + key2type.first);
      // dataprocessing_elements_type vector with SQL TYPES 
      dataprocessing_elements_types.push_back(enumToPrefix(key2type.second).sqltype);
    }
    // test for illegal entries
    // catch :
    const std::vector<String> bad_sym = {"+", "-", "?", "!", "*", "@", "%", "^", "&", "#", "=", "/", "\\", ":", "\"", "\'"};

    for (std::size_t idx = 0; idx != dataprocessing_elements.size(); ++idx)
    {
      // test for illegal SQL entries in dataprocessing_elements, if found replace
      for (const String& sym : bad_sym)
      {
        if( dataprocessing_elements[idx].hasSubstring(sym))
        // use String::quote
        {
          dataprocessing_elements[idx] = dataprocessing_elements[idx].quote();
          break;
        }
      }
    }





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
    const String features_table_stmt = createTable("FEATURES_TABLE", sql_stmt_features); 
    // 2. subordinates
    const String subordinates_table_stmt = createTable("FEATURES_SUBORDINATES", sql_stmt_subordinates); 
    // 3. dataprocessing
    const String dataprocessing_table_stmt = createTable("FEATURES_DATAPROCESSING", sql_stmt_dataprocessing); 



    String create_sql = \
      features_table_stmt + \
      
      subordinates_table_stmt + \
      
      " " + dataprocessing_table_stmt \
      
      // closing statement semicolon
      ;    






    // catch SQL contingencies
    //create_sql = create_sql.substitute("'", "\:"); // SQL escape single quote in strings  

    

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
      line_stmt =  "INSERT INTO FEATURES_TABLE (" + feature_elements_sql_stmt + ") VALUES (";
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
        line_stmt =  "INSERT INTO FEATURES_SUBORDINATES (" + subordinate_elements_sql_stmt + ") VALUES (";
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
    std::vector<String> dataproc_elems = {};
    std::vector<String> processing_action_names;



    dataproc_elems.push_back(static_cast<int64_t>(feature_map.getUniqueId() & ~(1ULL << 63)));
    const std::vector<DataProcessing> dataprocessing = feature_map.getDataProcessing();
  
    for (auto datap : dataprocessing)
    {
      dataproc_elems.push_back(datap.getSoftware().getName());
      dataproc_elems.push_back(datap.getCompletionTime().getDate());
      dataproc_elems.push_back(datap.getCompletionTime().getTime());

      const std::set<DataProcessing::ProcessingAction>& proc_ac = datap.getProcessingActions();

      for (const DataProcessing::ProcessingAction& a : proc_ac)
      {
        const String& action = DataProcessing::NamesOfProcessingAction[int(a)];
        processing_action_names.push_back(action);
      }
      if (processing_action_names.size() >= 1)
      {
        dataproc_elems.push_back(ListUtils::concatenate(processing_action_names, ",")); 
      }
      else
      {
        dataproc_elems.push_back(processing_action_names[0]);
      }
    }
   
    // userparam entries
    // dynamic type resolution 
    for (auto datap : dataprocessing_userparams)
    {
      datap.getKeys(dataproc_keys);
      for (auto key : dataproc_keys)
      {
        dataproc_elems.push_back(datap.getMetaValue(key));
      }
    }

    // non-userparam entries
    // resolve SQL type from dataprocessing_elements_types
    for (std::size_t idx = 0; idx != dataprocessing_elements.size(); ++idx)
    {
      //if (dataprocessing_elements_types[elem] == "TEXT")
      {
        if (dataprocessing_elements_types[idx] == "TEXT")
        {
          dataproc_elems[idx] = "'" + dataproc_elems[idx] + "'"; 
        }
      }
    }



    line_stmt =  "INSERT INTO FEATURES_DATAPROCESSING (" + dataprocessing_elements_sql_stmt + ") VALUES (";
    line_stmt += ListUtils::concatenate(dataproc_elems, ",");
    line_stmt += ");";




    //store in dataprocessing table
    conn.executeStatement(line_stmt);

  } // end of FeatureSQLFile::write



  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //                                                read FeatureMap as SQL database                                                      //
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // read SQL database and store as FeatureMap
  // fitted snippet of FeatureXMLFile::load
  // based on TransitionPQPFile::readPQPInput

  FeatureMap FeatureSQLFile::read(const std::string& filename) const
  {
    //create empty FeatureMap object
    FeatureMap feature_map;
    sqlite3 *db;
    sqlite3_stmt * stmt;
    std::string select_sql;

    // Open database
    SqliteConnector conn(filename);
    db = conn.getDB();

    // features
    bool features_exists = SqliteConnector::tableExists(db, "FEATURES_TABLE");
    if (features_exists)
    { 
      std::cout << std::endl;
      std::cout << "FEATURES_TABLE ok" << std::endl;
    }

    /*
      bool subordinates_exists = SqliteConnector::tableExists(db, "FEATURES_SUBORDINATES");
      if (subordinates_exists)
      {
        std::cout << std::endl;
        std::cout << "subordinates ok";
        // test if column FEATURE_REF exists  
        bool feature_ref = SqliteConnector::columnExists(db, "FEATURES_SUBORDINATES", "FEATURE_REF");
      }
        
      bool dataprocessing_exists = SqliteConnector::tableExists(db, "DATAPROCESSING");
      if (dataprocessing_exists)
      { 
        std::cout << std::endl;
        std::cout << "FEATURES_DATAPROCESSING ok";
      }   
    */

    //"FEATURES_SUBORDINATES" 
    //"FEATURES_DATAPROCESSING"

    // get feature table entries
    //SqliteConnector::executePreparedStatement(db, &stmt, "SELECT * FROM ;");
    String sql = "SELECT * FROM FEATURES_TABLE;";
    SqliteConnector::executePreparedStatement(db, &stmt, sql);
    sqlite3_step(stmt);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                                     store sqite3 database as FeatureMap                                              //
    //                                                     1. features                                                                      //                                    
    //                                                     2. subordinates                                                                  //
    //                                                     3. dataprocessing                                                                //
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /// 1. get feature data from database


    /// get subordinate ref_id vector and subordinate vector
    sql = "SELECT ref_id FROM FEATURES_SUBORDINATES ORDER BY ref_id ASC;";
    SqliteConnector::executePreparedStatement(db, &stmt, sql);
    sqlite3_step(stmt);

    std::vector<long> ref_ids = {}; //long for compatibility

    std::vector<Feature> subordinates = {};

    while (sqlite3_column_type( stmt, 0 ) != SQLITE_NULL)
    {
      Feature current_sub_feature;

      /// save ref_id vector
      String ref_id;
      long r_id = 0;
      // extract as String
      Sql::extractValue<String>(&ref_id, stmt, 0);
      r_id = stol(ref_id);
      ref_ids.push_back(r_id);
      //ref_ids.insert(r_id);



      /// save subordinates vector
      // set feature parameters
      String id_s;
      long id = 0;
      // extract as String
      Sql::extractValue<String>(&id_s, stmt, 0);
      id = stol(id_s);
    /*  
      //id = id_s.toInt();
      std::cout << id << std::endl;
      // store id for later use in subordinates
      feature_ids.insert(id);
    */
   
      double rt = 0.0;
      Sql::extractValue<double>(&rt, stmt, 2);
      double mz = 0.0;
      Sql::extractValue<double>(&mz, stmt, 3);
      double intensity = 0.0;
      Sql::extractValue<double>(&intensity, stmt, 4);
      int charge = 0;
      Sql::extractValue<int>(&charge, stmt, 5);
      //std::cout << charge << std::endl;
      double quality = 0.0;
      Sql::extractValue<double>(&quality, stmt, 6);

      // get values
      // id, RT, MZ, Intensity, Charge, Quality
      // save SQL column elements as feature
      current_sub_feature.setUniqueId(id);
      current_sub_feature.setRT(rt);
      current_sub_feature.setMZ(mz);
      current_sub_feature.setIntensity(intensity);
      current_sub_feature.setCharge(charge);
      current_sub_feature.setOverallQuality(quality);
      
      // access number of columns and infer type
      int cols = sqlite3_column_count(stmt);
      
      // print index and column names
      //std::cout << "columns : \t" << cols << std::endl; 

      // save userparam column names
      // read colum names, infer DataValue and write data to current_feature
      // with setMetaValue(value, datatype) DataValue::DataType
      
      String column_name;
      int column_type = 0;

      // spare first seven values with feature parameters
      // parse values with respective DataValue::DataType
      for (int i = 6; i < cols ; ++i)
      {
        column_name = sqlite3_column_name(stmt, i);
        column_type = getColumnDatatype(column_name);
        if(column_type == DataValue::STRING_VALUE)
        {
          String value;
          Sql::extractValue<String>(&value, stmt, i);
          current_sub_feature.setMetaValue(column_name, value); 
          continue;
        } else
        
        if(column_type == DataValue::INT_VALUE)
        {
          int value = 0;
          Sql::extractValue<int>(&value, stmt, i);
          current_sub_feature.setMetaValue(column_name, value); 
          continue;
        } else
        
        if(column_type == DataValue::DOUBLE_VALUE)
        {
          double value = 0.0;
          Sql::extractValue<double>(&value, stmt, i);          
          current_sub_feature.setMetaValue(column_name, value); 
          continue;
        } else
        
        if(column_type == DataValue::STRING_LIST)
        {
          String value; 
          Sql::extractValue<String>(&value, stmt, i);

          StringList sl;
          // cut off "[" and "]""
          value = value.chop(1);
          value = value.substr(1);
          value.split(", ", sl);
          current_sub_feature.setMetaValue(column_name, sl);
          continue;
        } else

        
        if(column_type == DataValue::INT_LIST)
        {
          String value; //IntList value;
          Sql::extractValue<String>(&value, stmt, i); //IntList
          value = value.chop(1);
          value = value.substr(1);
          std::vector<String> value_list;
          IntList il = ListUtils::create<int>(value, ',');
          current_sub_feature.setMetaValue(column_name, il);
          continue;
        } else
        
        if(column_type == DataValue::DOUBLE_LIST)
        {
          String value; //DoubleList value;
          Sql::extractValue<String>(&value, stmt, i); //DoubleList
          value = value.chop(1);
          value = value.substr(1);          
          DoubleList dl = ListUtils::create<double>(value, ',');
          current_sub_feature.setMetaValue(column_name, dl);
          continue;
        } else
      
        if(column_type == DataValue::EMPTY_VALUE)
        {
          String value;
          Sql::extractValue<String>(&value, stmt, i);
          continue;
        }
      }
    
      /// save subordinates vector
      subordinates.push_back(current_sub_feature);



      sqlite3_step(stmt);
    }

    /// output of subordinate ref_ids
    String ref_ids_line = ListUtils::concatenate(ref_ids, ",");
    std::cout << ref_ids_line << std::endl;




    /// traverse across features and add subordinate features if existent
    sql = "SELECT * FROM FEATURES_TABLE ORDER BY id ASC;";
    SqliteConnector::executePreparedStatement(db, &stmt, sql);
    sqlite3_step(stmt);
    


    //std::unordered_set<int> feature_ids = {}; //int64_t
    std::unordered_set<long> feature_ids = {}; //long for compatibility
    
    // set subordinate counter
    int ref_counter = 0;

    while (sqlite3_column_type( stmt, 0 ) != SQLITE_NULL)
    {
      Feature current_feature;

      // set feature parameters
      String id_s;
      long id = 0;
      // extract as String
      Sql::extractValue<String>(&id_s, stmt, 0);
      id = stol(id_s);
      //id = id_s.toInt();
      // line_sub.push_back(static_cast<int64_t>(sub_it->getUniqueId() & ~(1ULL << 63)));
      // extractValue<int> not working as expected
      // change on next pull request for SqliteConnector
    /*  
      //id = id_s.toInt();
      std::cout << id << std::endl;
      // store id for later use in subordinates
      feature_ids.insert(id);
    */
   
      double rt = 0.0;
      Sql::extractValue<double>(&rt, stmt, 1);
      double mz = 0.0;
      Sql::extractValue<double>(&mz, stmt, 2);
      double intensity = 0.0;
      Sql::extractValue<double>(&intensity, stmt, 3);
      int charge = 0;
      Sql::extractValue<int>(&charge, stmt, 4);
      //std::cout << charge << std::endl;
      double quality = 0.0;
      Sql::extractValue<double>(&quality, stmt, 5);

      // get values
      // id, RT, MZ, Intensity, Charge, Quality
      // save SQL column elements as feature
      current_feature.setUniqueId(id);
      current_feature.setRT(rt);
      current_feature.setMZ(mz);
      current_feature.setIntensity(intensity);
      current_feature.setCharge(charge);
      current_feature.setOverallQuality(quality);
      
      // access number of columns and infer type
      int cols = sqlite3_column_count(stmt);
      
      // print index and column names
      //std::cout << "columns : \t" << cols << std::endl; 

      // save userparam column names
      // read colum names, infer DataValue and write data to current_feature
      // with setMetaValue(value, datatype) DataValue::DataType
      
      String column_name;
      int column_type = 0;

      // spare first six values with feature parameters
      // parse values with respective DataValue::DataType
      // TODO: Integer values incorrectly typed as String
      // because of read out error
      for (int i = 6; i < cols ; ++i)
      {
        column_name = sqlite3_column_name(stmt, i);
        column_type = getColumnDatatype(column_name);
        if(column_type == DataValue::STRING_VALUE)
        {
          String value;
          Sql::extractValue<String>(&value, stmt, i);
          current_feature.setMetaValue(column_name, value); 
          continue;
        } else
        
        if(column_type == DataValue::INT_VALUE)
        {
          int value = 0;
          Sql::extractValue<int>(&value, stmt, i);
          current_feature.setMetaValue(column_name, value); 
          continue;
        } else
        
        if(column_type == DataValue::DOUBLE_VALUE)
        {
          double value = 0.0;
          Sql::extractValue<double>(&value, stmt, i);          
          current_feature.setMetaValue(column_name, value); 
          continue;
        } else
        
        if(column_type == DataValue::STRING_LIST)
        {
          String value; 
          Sql::extractValue<String>(&value, stmt, i);

          StringList sl;
          // cut off "[" and "]""
          value = value.chop(1);
          value = value.substr(1);
          value.split(", ", sl);
          current_feature.setMetaValue(column_name, sl);
          continue;
        } else

        
        if(column_type == DataValue::INT_LIST)
        {
          String value; //IntList value;
          Sql::extractValue<String>(&value, stmt, i); //IntList
          value = value.chop(1);
          value = value.substr(1);
          std::vector<String> value_list;
          IntList il = ListUtils::create<int>(value, ',');
          current_feature.setMetaValue(column_name, il);
          continue;
        } else
        
        if(column_type == DataValue::DOUBLE_LIST)
        {
          String value; //DoubleList value;
          Sql::extractValue<String>(&value, stmt, i); //DoubleList
          value = value.chop(1);
          value = value.substr(1);          
          DoubleList dl = ListUtils::create<double>(value, ',');
          current_feature.setMetaValue(column_name, dl);
          continue;
        } else
      
        if(column_type == DataValue::EMPTY_VALUE)
        {
          String value;
          Sql::extractValue<String>(&value, stmt, i);
          continue;
        }
      }

      //if (std::find(ref_ids.begin(), ref_ids.end(), id) != ref_ids.end())
      if (ref_ids[ref_counter] == id)
      {
        while (ref_ids[ref_counter] == ref_ids[ref_counter + 1])
        {
          // add entries to feature subordinate
          
          std::vector<Feature> current_subordinate;
          current_subordinate.push_back(subordinates[ref_counter]);
          current_feature.setSubordinates(current_subordinate);
          std::cout << id << " is in " << std::endl;
          std::cout << ref_counter << " is in " << std::endl;
        
          // increase counter for subordinate vector index
          ++ref_counter;
        }
        ++ref_counter;
      }
    /*
       else
      {
        std::cout << id << " is out " << std::endl;
      }
    */
      // save feature in FeatureMap object
      feature_map.push_back(current_feature);
      feature_map.ensureUniqueId();
      
      //result.push_back( sqlite3_column_int(stmt, 0) );
      sqlite3_step(stmt);


      std::cout << ref_counter << " is in " << std::endl;

    }


    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// 2. get subordinate data from database
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



/*
    sql = "SELECT * FROM FEATURES_SUBORDINATES;";
    SqliteConnector::executePreparedStatement(db, &stmt, sql);
    sqlite3_step(stmt);
  

    while (sqlite3_column_type( stmt, 0 ) != SQLITE_NULL)
    {
      String subord_id;
      long sub_id = 0;
      // extract as String
      Sql::extractValue<String>(&subord_id, stmt, 0);
      sub_id = stol(subord_id);

      String ref_id;
      long r_id = 0;
      // extract as String
      Sql::extractValue<String>(&subord_id, stmt, 1);
      r_id = stol(subord_id);

      double rt = 0.0;
      Sql::extractValue<double>(&rt, stmt, 2);
      
      double mz = 0.0;
      Sql::extractValue<double>(&mz, stmt, 3);
      
      double intensity = 0.0;
      Sql::extractValue<double>(&intensity, stmt, 4);
      
      int charge = 0;
      Sql::extractValue<int>(&charge, stmt, 5);
      
      double quality = 0.0;
      Sql::extractValue<double>(&quality, stmt, 6);


      std::unordered_set<long>::const_iterator subordinate_in_feature = feature_ids.find(r_id);
      if (subordinate_in_feature == feature_ids.end())
      {
        std::cout << r_id << " not found " << std::endl;
      } else
      {
        std::cout << r_id << " is in " << std::endl;
      }
      sqlite3_step(stmt);
    }
  

*/



    /*

        // Iteration over FeatureMap
        for (auto it = map.begin(); it != map.end(); ++it)
        {
          cout << it->getRT() << " - " << it->getMZ() << endl;
        }


    */ 



    /*   
      // get values
      // id, RT, MZ, Intensity, Charge, Quality
      // save SQL column elements as feature
      current_feature.setUniqueId(sub_id);
      current_feature.setRT(rt);
      current_feature.setMZ(mz);
      current_feature.setIntensity(intensity);
      current_feature.setCharge(charge);
      current_feature.setOverallQuality(quality);

    

      // access number of columns and infer type
      int cols = sqlite3_column_count(stmt);
    
      // print out index and column names
      //std::cout << "columns : \t" << cols << std::endl; 

      // save userparam column names
      // read colum names, infer DataValue and write data to current_feature
      // with setMetaValue(value, datatype) DataValue::DataType

    

      //result.push_back( sqlite3_column_int(stmt, 0) );
      sqlite3_step(stmt);
    }
    */

  sqlite3_finalize(stmt);
  // close SQL database connection
  

  // return FeatureMap 
  return feature_map;
  }


  // end of FeatureSQLFile::read
} // namespace OpenMS



















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
*/



/*
SNIPPETS BLOCK

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








    if (subordinate_table_exists)
    {
      std::cout << "subs here" << std::endl;
    }
    else
    {
      std::cout << "subs missing" << std::endl;
    }





    std::cout << std::endl;
    for(auto elem : dataproc_map_key2type)
    {
      std::cout << elem.first << "\t " << elem.second << "\n"; 
    }
    std::cout << std::endl;
    std::cout << dataproc_map_key2type.size() << "\n";
























*/