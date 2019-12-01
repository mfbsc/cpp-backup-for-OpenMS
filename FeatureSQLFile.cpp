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
// $Maintainer: Timo Sachsenberg $
// $Authors: Matthias Fuchs $
// --------------------------------------------------------------------------


#include <OpenMS/CONCEPT/Exception.h>
#include <OpenMS/CONCEPT/UniqueIdInterface.h>

#include <OpenMS/DATASTRUCTURES/DBoundingBox.h>
#include <OpenMS/DATASTRUCTURES/DateTime.h>
#include <OpenMS/DATASTRUCTURES/DataValue.h>
#include <OpenMS/DATASTRUCTURES/ListUtils.h>
#include <OpenMS/DATASTRUCTURES/Param.h>
#include <OpenMS/DATASTRUCTURES/String.h>

#include <OpenMS/FORMAT/FeatureSQLFile.h>
#include <OpenMS/FORMAT/FileHandler.h>
#include <OpenMS/FORMAT/SqliteConnector.h>

#include <OpenMS/KERNEL/FeatureMap.h>
#include <OpenMS/KERNEL/ConsensusMap.h>
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

#include <typeinfo>

// #include <Boost>

using namespace std;

namespace OpenMS
{ 
  namespace Sql = Internal::SqliteHelper;

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // helper functions                                                                               //
  ////////////////////////////////////////////////////////////////////////////////////////////////////

  // store functions
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
  DataValue::DataType getColumnDatatype(const  String& label)
  { 
    DataValue::DataType type;
    if (label.hasPrefix("_S_"))
    {
      type = DataValue::STRING_VALUE;
    } 
    else if (label.hasPrefix("_I_"))
    {
      type = DataValue::INT_VALUE;
    } 
    else if (label.hasPrefix("_D_"))
    {
      type = DataValue::DOUBLE_VALUE;      
    } 
    else if (label.hasPrefix("_SL_"))
    {
      type = DataValue::STRING_LIST;
    } 
    else if (label.hasPrefix("_IL_"))
    {
      type = DataValue::INT_LIST;
    } 
    else if (label.hasPrefix("_DL_"))
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

  /*
    // get userparameter
    String getColumnName(const String& label)
    { 
      String label_userparam;
      if (label.hasPrefix("_S_"))
      {
        label_userparam = label.substr(3);
      } 
      else if (label.hasPrefix("_I_"))
      {
        label_userparam = label.substr(3);
      } 
      else if (label.hasPrefix("_D_"))
      {
        label_userparam = label.substr(3);
      } 
      else if (label.hasPrefix("_SL_"))
      {
        label_userparam = label.substr(4);
      } 
      else if (label.hasPrefix("_IL_"))
      {
        label_userparam = label.substr(4);
      } 
      else if (label.hasPrefix("_DL_"))
      {
        label_userparam = label.substr(4);
      } else 
      {
        label_userparam = label;
        
        //if (!label.hasPrefix("_"))
        //{
        //  datatype = 6;
        //}
        
      }
      return label_userparam;
    }
  */

  // write userparameter to feature
  void setUserParams(Feature& current_feature, String &column_name, const DataValue::DataType column_type, int i, sqlite3_stmt* stmt)
  {
    if (column_type == DataValue::STRING_VALUE)
    {
      column_name = column_name.substr(3);
      String value;
      Sql::extractValue<String>(&value, stmt, i);
      current_feature.setMetaValue(column_name, value); 
    } 
    else if (column_type == DataValue::INT_VALUE)
    {
      column_name = column_name.substr(3);
      int value = 0;
      Sql::extractValue<int>(&value, stmt, i);
      current_feature.setMetaValue(column_name, value); 
    } 
    else if (column_type == DataValue::DOUBLE_VALUE)
    {
      column_name = column_name.substr(3);
      double value = 0.0;
      Sql::extractValue<double>(&value, stmt, i);          
      current_feature.setMetaValue(column_name, value); 
    } 
    else if (column_type == DataValue::STRING_LIST)
    {
      column_name = column_name.substr(4);
      String value; 
      Sql::extractValue<String>(&value, stmt, i);

      StringList sl;
      // cut off "[" and "]""
      value = value.chop(1);
      value = value.substr(1);
      value.split(", ", sl);
      current_feature.setMetaValue(column_name, sl);
    } 
    else if (column_type == DataValue::INT_LIST)
    {
      column_name = column_name.substr(4);
      String value; //IntList value;
      Sql::extractValue<String>(&value, stmt, i); //IntList
      value = value.chop(1);
      value = value.substr(1);
      std::vector<String> value_list;
      IntList il = ListUtils::create<int>(value, ',');
      current_feature.setMetaValue(column_name, il);
    } 
    else if (column_type == DataValue::DOUBLE_LIST)
    {
      column_name = column_name.substr(4);
      String value; //DoubleList value;
      Sql::extractValue<String>(&value, stmt, i); //DoubleList
      value = value.chop(1);
      value = value.substr(1);          
      DoubleList dl = ListUtils::create<double>(value, ',');
      current_feature.setMetaValue(column_name, dl);
    } 
    else if (column_type == DataValue::EMPTY_VALUE)
    {
      String value;
      Sql::extractValue<String>(&value, stmt, i);
    }
  }

  // probe first feature in feature_map as template to set tables
  // check once for features, subordinates, dataprocessing, convexhull bboxes respectively 
  std::tuple<bool, bool, bool, bool, bool> getTables(const FeatureMap& feature_map)
  {
    bool features_switch, subordinates_switch, dataprocessing_switch, features_bbox_switch, subordinates_bbox_switch;
    features_switch = false;
    features_bbox_switch = false;
    subordinates_switch = false;
    subordinates_bbox_switch = false;
    dataprocessing_switch = false;

    if (feature_map.getDataProcessing().size() > 0)
    {
      dataprocessing_switch = true;
    }

    for (auto feature : feature_map)
    {
      features_switch = true;
      if (feature.getConvexHulls().size() > 0)
      {
        features_bbox_switch = true;
      }
      for (Feature sub : feature.getSubordinates())
      {
        subordinates_switch = true;
        if (sub.getConvexHulls().size() > 0)
        {
          subordinates_bbox_switch = true;
        }
      }
    }
    return std::make_tuple(features_switch, subordinates_switch, dataprocessing_switch, features_bbox_switch, subordinates_bbox_switch);
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  //write FeatureMap as SQL database                                                      //
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // fitted snippet of FeatureXMLFile::load
  void FeatureSQLFile::write(const std::string& out_fm, const FeatureMap& feature_map) const
  {
    String path="/home/mkf/Development/OpenMS/src/tests/class_tests/openms/data/";
    String filename = path.append(out_fm);
    
    // delete file if present
    File::remove(filename);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //variable declaration                                                          //
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////   

    // declare table entries
    std::vector<String> feature_elements = {"ID", "RT", "MZ", "Intensity", "Charge", "Quality"};
    std::vector<String> feature_elements_types = {"INTEGER", "REAL", "REAL", "REAL", "INTEGER", "REAL"};
    
    std::vector<String> subordinate_elements = {"ID", "SUB_IDX" , "REF_ID", "RT", "MZ", "Intensity", "Charge", "Quality"};
    std::vector<String> subordinate_elements_types = {"INTEGER", "INTEGER", "INTEGER", "REAL", "REAL", "REAL", "INTEGER", "REAL"};

    std::vector<String> dataprocessing_elements = {"ID", "SOFTWARE", "SOFTWARE_VERSION", "DATA", "TIME", "ACTIONS"};
    std::vector<String> dataprocessing_elements_types = {"INTEGER" ,"TEXT" ,"TEXT" ,"TEXT" ,"TEXT" , "TEXT"};

    std::vector<String> feat_bounding_box_elements = {"REF_ID", "MIN_MZ", "MIN_RT", "MAX_MZ", "MAX_RT", "BB_IDX"};
    std::vector<String> feat_bounding_box_elements_types = {"INTEGER" ,"REAL" ,"REAL" ,"REAL" , "REAL", "INTEGER"};

    std::vector<String> sub_bounding_box_elements = {"ID", "REF_ID", "MIN_MZ", "MIN_RT", "MAX_MZ", "MAX_RT", "BB_IDX"};
    std::vector<String> sub_bounding_box_elements_types = {"INTEGER" ,"INTEGER" ,"REAL" ,"REAL" ,"REAL" , "REAL", "INTEGER"};

    // get used tables
    auto tables = getTables(feature_map);
    bool features_switch = std::get<0>(tables);
    bool subordinates_switch = std::get<1>(tables);
    bool dataprocessing_switch = std::get<2>(tables);
    bool features_bbox_switch = std::get<3>(tables);
    bool subordinates_bbox_switch = std::get<4>(tables);

    //DEBUG
    //std::cout << "table declaration of switches to see which tables are in or out" << std::endl;
    //std::cout << features_switch << subordinates_switch << dataprocessing_switch << features_bbox_switch << subordinates_bbox_switch << std::endl;
    //std::cout << "table declaration of switches to see which tables are in or out" << std::endl;

    // read feature_map userparameter values and store as key value map
    // pass all CommonMetaKeys by setting frequency to 0.0 to a set 
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

    // generate vector dataproc_keys
    // fill vector with dataprocessing_userparameters
    // define map with corresponding datatype, dataproc_map_key2type
    // define String vector sequenc_keys, storing succession of userparameters keys in feature_map
    std::vector<String> dataproc_keys;
    const std::vector<DataProcessing> dataprocessing_userparams = feature_map.getDataProcessing();
    std::map<String, DataValue::DataType> dataproc_map_key2type;
    std::vector<String> sequence_keys;
    // traverse each dataprocessing entry in dataprocessing_userparams
    for (auto datap : dataprocessing_userparams)
    {
      // get key of current datap entry
      datap.getKeys(dataproc_keys);
      for (auto key : dataproc_keys)
      {
        const DataValue::DataType& dt = datap.getMetaValue(key).valueType();  // get DataType of current key
        dataproc_map_key2type[key] = dt; // save key, datatype pair
        sequence_keys.push_back(key); // store sequence of dataprocessing keys for reuse in table generation
      }
    }

    // get number  of user parameters to derive explicit NULL value entries for empty features 
    int user_param_null_entries = common_keys.size();

    std::vector<String> null_entries(user_param_null_entries, "NULL");
    String null_entry_line = ListUtils::concatenate(null_entries, ",");
    std::cout << null_entry_line << std::endl;

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// build feature header for sql table                                                            //                                      //
    /// build subordinate header as sql table                                                         //                                      //
    /// build dataprocessing header as sql table                                                      //                                      //
    /// build boundingbox header as sql table                                                         //                                      //
    ////////////////////////////////////////////////////////////////////////////////////////////////////
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
    for (auto & key : dataproc_keys)
    { 
      //DEBUG
      //std::cout << "\"" << enumToPrefix(dataproc_map_key2type[key]).prefix << key << "\"" << std::endl;
      //std::cout << enumToPrefix(dataproc_map_key2type[key]).prefix << std::endl;
      //std::cout << key << std::endl;
      
      // version without " to keep illegal entrie catch
      String dataproc_element = enumToPrefix(dataproc_map_key2type[key]).prefix + key;
      // all in one version with "
      //String dataproc_element = "\"" + enumToPrefix(dataproc_map_key2type[key]).prefix + key + "\"";
      //std::cout << dataproc_element << std::endl;
      dataprocessing_elements.push_back(dataproc_element);
      String dataproc_element_type = enumToPrefix(dataproc_map_key2type[key]).sqltype;
      //std::cout << dataproc_element_type << std::endl;
      dataprocessing_elements_types.push_back(dataproc_element_type);
      //std::cout << "\"" << enumToPrefix(dataproc_map_key2type[key]).prefix << key << "\" " << enumToPrefix(dataproc_map_key2type[key]).sqltype << std::endl;      
    }


    // test for illegal entries
    // catch :
    const std::vector<String> bad_sym = {"+", "_", "-", "?", "!", "*", "@", "%", "^", "&", "#", "=", "/", "\\", ":", "\"", "\'"};

    for (std::size_t idx = 0; idx != dataprocessing_elements.size(); ++idx)
    {
      // test for illegal SQL entries in dataprocessing_elements, if found replace
      for (const String& sym : bad_sym)
      {
        if (dataprocessing_elements[idx].hasSubstring(sym))
        // use String::quote
        {
          dataprocessing_elements[idx] = dataprocessing_elements[idx].quote();
          break;
        }
      }
    }

    // boundingbox is part of convexhull is part of feature and subordinates
    // points of MZ/RT-min-/maxima saved as quadrupel of doubles per convexhull number
    // preparation of boundingbox header atm needs no implementation


    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // create database with empty tables                                                //
    // construct SQL_labels for feature, subordinate, dataprocessing and boundingbox tables
    ////////////////////////////////////////////////////////////////////////////////////////////////////
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
    //std::for_each(sql_labels_features.begin(), sql_labels_features.end(), [] (String &s) {s.append(" NULL");});
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
    //std::for_each(sql_labels_subordinates.begin(), sql_labels_subordinates.end(), [] (String &s) {s.append(" NOT NULL");});
    // create single string with delimiter ',' as TABLE header template
    String sql_stmt_subordinates = ListUtils::concatenate(sql_labels_subordinates, ",");


    // 3. dataprocessing table
    std::vector<String> sql_labels_dataprocessing = {};
    for (std::size_t idx = 0; idx != dataprocessing_elements.size(); ++idx)
    { 
      sql_labels_dataprocessing.push_back(dataprocessing_elements[idx] + " " + dataprocessing_elements_types[idx]);
    }

    //DEBUG
    //String dataproc_keys_label_outputstring = ListUtils::concatenate(sql_labels_dataprocessing, "\n");
    //std::cout << "\n\n\n\n\n\n\n\n" << dataproc_keys_label_outputstring << std::endl;


    // add PRIMARY KEY to first entry, assuming 1st column contains primary key
    sql_labels_dataprocessing[0].append(" PRIMARY KEY");
    // add "NOT NULL" to all entries
    //std::for_each(sql_labels_dataprocessing.begin(), sql_labels_dataprocessing.end(), [] (String &s) {s.append(" NOT NULL");});
    // create single string with delimiter ',' as TABLE header template
    String sql_stmt_dataprocessing = ListUtils::concatenate(sql_labels_dataprocessing, ",");


    // 4. boundingbox table
    // 4.1 features
    std::vector<String> sql_labels_boundingbox = {};
    for (std::size_t idx = 0; idx != feat_bounding_box_elements.size(); ++idx)
    {
      sql_labels_boundingbox.push_back(feat_bounding_box_elements[idx] + " " + feat_bounding_box_elements_types[idx]);
    }
    // no PRIMARY KEY because of executeStatement's UNIQUE constraint for primary keys
    // add "NOT NULL" to all entries
    //std::for_each(sql_labels_boundingbox.begin(), sql_labels_boundingbox.end(), [] (String &s) {s.append(" NOT NULL");});
    // create single string with delimiter ',' as TABLE header template
    String sql_stmt_feat_boundingbox = ListUtils::concatenate(sql_labels_boundingbox, ",");


    // 4. boundingbox table
    // 4.2 subordinates
    sql_labels_boundingbox = {};
    for (std::size_t idx = 0; idx != sub_bounding_box_elements.size(); ++idx)
    {
      sql_labels_boundingbox.push_back(sub_bounding_box_elements[idx] + " " + sub_bounding_box_elements_types[idx]);
    }
    // add PRIMARY KEY to first entry, assuming 1st column contains primary key
    //sql_labels_boundingbox[0].append(" PRIMARY KEY");
    // add "NOT NULL" to all entries
    //std::for_each(sql_labels_boundingbox.begin(), sql_labels_boundingbox.end(), [] (String &s) {s.append(" NOT NULL");});
    // create single string with delimiter ',' as TABLE header template
    String sql_stmt_sub_boundingbox = ListUtils::concatenate(sql_labels_boundingbox, ",");


    // create empty SQL table stmt
    String features_table_stmt, subordinates_table_stmt, dataprocessing_table_stmt, feature_boundingbox_table_stmt, subordinate_boundingbox_table_stmt;
    // 1. features
    if (features_switch == true)
    {
      features_table_stmt = createTable("FEATURES_TABLE", sql_stmt_features);
    } 
    else if (features_switch == false)
    {
      features_table_stmt = "";
    }
    // 2. subordinates
    if (subordinates_switch == true)
    {
      subordinates_table_stmt = createTable("FEATURES_SUBORDINATES", sql_stmt_subordinates);
    } 
    else if (subordinates_switch == false)
    {
      subordinates_table_stmt = "";
    }
    // 3. dataprocessing
    if (dataprocessing_switch == true)
    {
      dataprocessing_table_stmt = createTable("FEATURES_DATAPROCESSING", sql_stmt_dataprocessing);
    } 
    else if (dataprocessing_switch == false)
    {
      dataprocessing_table_stmt = "";
    }
    // 4. boundingbox (features & subordinates)
    // features
    if (features_bbox_switch == true)
    {
      feature_boundingbox_table_stmt = createTable("FEATURES_TABLE_BOUNDINGBOX", sql_stmt_feat_boundingbox);
    } 
    else if (features_bbox_switch == false)
    {
      feature_boundingbox_table_stmt = "";
    }
    // subordinates
    if (subordinates_bbox_switch)
    {
      subordinate_boundingbox_table_stmt = createTable("SUBORDINATES_TABLE_BOUNDINGBOX", sql_stmt_sub_boundingbox);
    } 
    else if (subordinates_bbox_switch == false)
    {
      subordinate_boundingbox_table_stmt = "";
    }


    String create_sql = \
      features_table_stmt + \
      subordinates_table_stmt + \
      dataprocessing_table_stmt + \
      feature_boundingbox_table_stmt + \
      subordinate_boundingbox_table_stmt \
      ;    

    // catch SQL contingencies
    //create_sql = create_sql.substitute("'", "\:"); // SQL escape single quote in strings  
    // create SQL database, create empty SQL tables  see (https://github.com/OpenMS/OpenMS/blob/develop/src/openms/source/FORMAT/OSWFile.cpp)
    // Open connection to database
    SqliteConnector conn(filename);
    conn.executeStatement(create_sql);




    // break into functions?
    // turn into line feed function for SQL database input step statement
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // store FeatureMap data in tables                                                                //
    // 1. features                                                                                    //                                    
    // 2. feature boundingboxes                                                                       //                                    
    // 3. subordinates                                                                                //
    // 4. subordinate boundingboxes                                                                   //
    // 5. dataprocessing                                                                              //
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// 1. insert data of features table
    const String feature_elements_sql_stmt = ListUtils::concatenate(feature_elements, ","); 

    const String feat_bbox_elements_sql_stmt = ListUtils::concatenate(feat_bounding_box_elements, ","); 

    // 1.
    if (features_switch)
    // TODO Error handling for empty features?
    {
      for (const Feature& feature : feature_map)
      {
        String line_stmt_features_table;
        std::vector<String> line;


        int64_t id = static_cast<int64_t>(feature.getUniqueId() & ~(1ULL << 63));
        //int64_t id = feature.getUniqueId();


        line.push_back(id);
        line.push_back(feature.getRT());
        line.push_back(feature.getMZ());
        line.push_back(feature.getIntensity());
        line.push_back(feature.getCharge());
        line.push_back(feature.getOverallQuality());
        // catch features without user params
        if (feature.isMetaEmpty())
        {
          std::cout << "has no userparams" << std::endl;

          line_stmt_features_table =  "INSERT INTO FEATURES_TABLE (" + feature_elements_sql_stmt + ") VALUES (";
          line_stmt_features_table += ListUtils::concatenate(line, ",");
          line_stmt_features_table += ",";
          line_stmt_features_table += null_entry_line;
          line_stmt_features_table += ");";
          //store in features table
          conn.executeStatement(line_stmt_features_table);

          continue;
        }
        else if (!feature.isMetaEmpty())
        {
          std::cout << "has userparams" << std::endl;

          for (const String& key : common_keys)
          {
            String s = feature.getMetaValue(key);

            if (s.length() == 0)
            {
              s = "NULL";
            }

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
          line_stmt_features_table =  "INSERT INTO FEATURES_TABLE (" + feature_elements_sql_stmt + ") VALUES (";
          line_stmt_features_table += ListUtils::concatenate(line, ",");
          line_stmt_features_table += ");";
          //store in features table
          conn.executeStatement(line_stmt_features_table);

          continue;
        }
        

      }
    }

    // 2.
    if (features_bbox_switch)
    {

      double min_MZ;
      double min_RT;
      double max_MZ;
      double max_RT;

      // fetch convexhull of feature for bounding box data
      for (const Feature& feature : feature_map)
      {
        /// insert boundingbox data to table FEATURES_TABLE_BOUNDINGBOX
        std::vector<String> feat_bbox_line;

        int bbox_idx = 0;

        int64_t id = static_cast<int64_t>(feature.getUniqueId() & ~(1ULL << 63));
        //int64_t id = feature.getUniqueId();

        // add bbox entries of current convexhull until all cvhulls are visited
        for (Size b = 0; b < feature.getConvexHulls().size(); ++b)
        {
          //clear vector line for current bbox
          feat_bbox_line.clear();
          String line_stmt_features_table_boundingbox =  "INSERT INTO FEATURES_TABLE_BOUNDINGBOX (" + feat_bbox_elements_sql_stmt + ") VALUES (";
          bbox_idx = b;

          min_MZ = feature.getConvexHulls()[b].getBoundingBox().minX();
          min_RT = feature.getConvexHulls()[b].getBoundingBox().minY();
          max_MZ = feature.getConvexHulls()[b].getBoundingBox().maxX();
          max_RT = feature.getConvexHulls()[b].getBoundingBox().maxY();

          std::cout << "All boundingbox value for all current feature" << b  << min_MZ << "\t" << min_RT << "\t" << max_MZ << "\t" << max_RT << std::endl;

          feat_bbox_line.push_back(id);
          feat_bbox_line.insert(feat_bbox_line.end(), {min_MZ,min_RT,max_MZ,max_RT});
          feat_bbox_line.push_back(bbox_idx);

          line_stmt_features_table_boundingbox += ListUtils::concatenate(feat_bbox_line, ",");
          //store String in FEATURES_TABLE_BOUNDINGBOX
          line_stmt_features_table_boundingbox += ");";
          conn.executeStatement(line_stmt_features_table_boundingbox);
        }
      }
    }

    // 3.
    if (subordinates_switch)
    {
      /// 2. insert data of subordinates table
      String line_stmt;
      const String subordinate_elements_sql_stmt = ListUtils::concatenate(subordinate_elements, ","); 
      //for (auto feature : feature_map)
      for (const Feature& feature : feature_map)
      {
        int sub_idx = 0;
        std::vector<String> line;
        for (const Feature& sub : feature.getSubordinates())
        {       

          line.push_back(static_cast<int64_t>(sub.getUniqueId() & ~(1ULL << 63)));
          //int64_t id = sub.getUniqueId();
          //line.push_back(id);

          // additional index value to preserve order of subordinates
          line.push_back(sub_idx);
          ++sub_idx;

          //line.push_back(static_cast<int64_t>(feature.getUniqueId() & ~(1ULL << 63)));
          line.push_back(feature.getUniqueId());


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
    }

    // 4.
    if (subordinates_bbox_switch)
    {
      // fetch bounding box data of subordinate convexhull
      const String sub_bbox_elements_sql_stmt = ListUtils::concatenate(sub_bounding_box_elements, ","); 
      for (const Feature& feature : feature_map)
      {
        int64_t ref_id = static_cast<int64_t>(feature.getUniqueId() & ~(1ULL << 63));
        //int64_t ref_id = feature.getUniqueId();


        for (const Feature& sub : feature.getSubordinates())
        {
          int64_t id = static_cast<int64_t>(sub.getUniqueId() & ~(1ULL << 63));
          //int64_t id = sub.getUniqueId();

          /// insert boundingbox data to table SUBORDINATES_TABLE_BOUNDINGBOX
          std::vector<String> bbox_line;

          double min_MZ;
          double min_RT;
          double max_MZ;
          double max_RT;

          int bbox_idx = 0;

          // add bbox entries of current convexhull
          for (Size b = 0; b < sub.getConvexHulls().size(); ++b)
          {
            //clear vector line for current bbox
            bbox_line.clear();
            String line_stmt_subordinates_table_boundingbox =  "INSERT INTO SUBORDINATES_TABLE_BOUNDINGBOX (" + sub_bbox_elements_sql_stmt + ") VALUES (";
            bbox_idx = b;

            min_MZ = sub.getConvexHulls()[b].getBoundingBox().minX();
            min_RT = sub.getConvexHulls()[b].getBoundingBox().minY();
            max_MZ = sub.getConvexHulls()[b].getBoundingBox().maxX();
            max_RT = sub.getConvexHulls()[b].getBoundingBox().maxY();

            bbox_line.push_back(id);
            bbox_line.push_back(ref_id);
            bbox_line.insert(bbox_line.end(), {min_MZ,min_RT,max_MZ,max_RT});
            bbox_line.push_back(bbox_idx);

            line_stmt_subordinates_table_boundingbox += ListUtils::concatenate(bbox_line, ",");
            line_stmt_subordinates_table_boundingbox += ");";
            conn.executeStatement(line_stmt_subordinates_table_boundingbox);
          }
        }
      }
    }

    // 5.
    if (dataprocessing_switch)
    {
      /// 3. insert labels of sql_stmt (INSERT INTO FEATURE_DATAPROCESSING (...))
      const String dataprocessing_elements_sql_stmt = ListUtils::concatenate(dataprocessing_elements, ",");

      std::vector<String> dataproc_elems = {};
      std::vector<String> processing_action_names;
      
      // get feature_map ID
      size_t dp_id = feature_map.getUniqueId();
      std::cout << "\n" << "This is correct dp_id ? " <<  dp_id << std::endl;
      size_t dp_id63 = static_cast<size_t>(feature_map.getUniqueId() & ~(1ULL << 63));
      std::cout << "\n" << "This is correct dp_id63 ? " <<  dp_id63 << std::endl;
    
      // add featureMap object ID to dataproc_elems
      dataproc_elems.push_back(static_cast<int64_t>(feature_map.getUniqueId() & ~(1ULL << 63)));
      
      const std::vector<DataProcessing> dataprocessing = feature_map.getDataProcessing();
    
      for (auto datap : dataprocessing)
      {
        // add default values of dataprocessing entry
        dataproc_elems.push_back(datap.getSoftware().getName());
        dataproc_elems.push_back(datap.getSoftware().getVersion());
        dataproc_elems.push_back(datap.getCompletionTime().getDate());
        dataproc_elems.push_back(datap.getCompletionTime().getTime());


        // get processingAction entries
        const std::set<DataProcessing::ProcessingAction>& proc_ac = datap.getProcessingActions();
        for (const DataProcessing::ProcessingAction& a : proc_ac)
        {
          const String& action = DataProcessing::NamesOfProcessingAction[int(a)];
          processing_action_names.push_back(action);
        }
        if (processing_action_names.size() >= 1)
        {
          dataproc_elems.push_back(ListUtils::concatenate(processing_action_names, ",")); 
        } else
        {
          dataproc_elems.push_back(processing_action_names[0]);
        }
      }
    

      // userparam entries
      for (auto datap : dataprocessing_userparams)
      {
        datap.getKeys(dataproc_keys);
        for (auto& key : dataproc_keys)
        {
          //std::cout << "key : " << key << "\t with value " << datap.getMetaValue(key) << std::endl;
          dataproc_elems.push_back(datap.getMetaValue(key));
        }
      }


      // non-userparam entries
      // resolve SQL type from dataprocessing_elements_types
      for (std::size_t idx = 0; idx != dataprocessing_elements.size(); ++idx)
      {
        if (dataprocessing_elements_types[idx] == "TEXT")
        {
          dataproc_elems[idx] = "'" + dataproc_elems[idx] + "'"; 
        }
      }
      String line_stmt =  "INSERT INTO FEATURES_DATAPROCESSING (" + dataprocessing_elements_sql_stmt + ") VALUES (";
      line_stmt += ListUtils::concatenate(dataproc_elems, ",");
      line_stmt += ");";

      //store in dataprocessing table
      conn.executeStatement(line_stmt);
    }

  } // end of FeatureSQLFile::write

















  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // read functions                                                                                 //
  ////////////////////////////////////////////////////////////////////////////////////////////////////


  // read BBox values of convex hull entries for feature, subordinate table
  ConvexHull2D readBBox_(sqlite3_stmt * stmt, int column_nr)
  {
      
    double min_mz = 0.0;
    Sql::extractValue<double>(&min_mz, stmt, (column_nr + 1));
    double min_rt = 0.0;
    Sql::extractValue<double>(&min_rt, stmt, (column_nr + 2));
    double max_mz = 0.0;
    Sql::extractValue<double>(&max_mz, stmt, (column_nr + 3));
    double max_rt = 0.0;
    Sql::extractValue<double>(&max_rt, stmt, (column_nr + 4));

    ConvexHull2D hull;
    hull.addPoint({min_mz, min_rt});
    hull.addPoint({max_mz, max_rt});
    return hull;
  }


  // get values of subordinate, user parameter entries of subordinate table
  Feature readSubordinate_(sqlite3_stmt * stmt, int column_nr, int cols_features, int cols_subordinates, bool has_bbox)
  {
    Feature subordinate;

    String id_string;
    Sql::extractValue<String>(&id_string, stmt, cols_features);
    std::istringstream iss(id_string);
    int64_t id_sub = 0;
    iss >> id_sub;

    std::cout << "  col_sub is : " << cols_features << std::endl;
    std::cout << "  col_sub value is : " << id_sub << std::endl;

    double rt = 0.0;
    Sql::extractValue<double>(&rt, stmt, column_nr + 1);
    double mz = 0.0;
    Sql::extractValue<double>(&mz, stmt, column_nr + 2);
    double intensity = 0.0;
    Sql::extractValue<double>(&intensity, stmt, column_nr + 3);
    int charge = 0;
    Sql::extractValue<int>(&charge, stmt, column_nr + 4);
    double quality = 0.0;
    Sql::extractValue<double>(&quality, stmt, column_nr + 5);
    subordinate.setUniqueId(id_sub);
    subordinate.setRT(rt);
    subordinate.setMZ(mz);
    subordinate.setIntensity(intensity);
    subordinate.setCharge(charge);
    subordinate.setOverallQuality(quality);

    // userparams
    column_nr = cols_features + 5;  // + 5 = subordinate params
    for (int i = column_nr; i < cols_features + cols_subordinates  ; ++i) // subordinate userparams - 1 = index of last element in cols_subordinates
    {
      String column_name = sqlite3_column_name(stmt, i);
      int column_type = getColumnDatatype(column_name);

      if (sqlite3_column_type(stmt,i) == SQLITE_NULL)
      {
        break;
      }
      else if (column_type == DataValue::STRING_VALUE)
      {
        column_name = column_name.substr(3);
        String value;
        Sql::extractValue<String>(&value, stmt, i);
        subordinate.setMetaValue(column_name, value); 
        continue;
      } 
      else if (column_type == DataValue::INT_VALUE)
      {
        column_name = column_name.substr(3);
        int value = 0;
        Sql::extractValue<int>(&value, stmt, i);
        subordinate.setMetaValue(column_name, value); 
        continue;
      } 
      else if (column_type == DataValue::DOUBLE_VALUE)
      {
        column_name = column_name.substr(3);
        double value = 0.0;
        Sql::extractValue<double>(&value, stmt, i);          
        subordinate.setMetaValue(column_name, value); 
        continue;
      } 
      else if (column_type == DataValue::STRING_LIST)
      {
        column_name = column_name.substr(4);
        String value; 
        Sql::extractValue<String>(&value, stmt, i);

        StringList sl;
        // cut off "[" and "]""
        value = value.chop(1);
        value = value.substr(1);
        value.split(", ", sl);
        subordinate.setMetaValue(column_name, sl);
        continue;
      } 
      else if (column_type == DataValue::INT_LIST)
      {
        column_name = column_name.substr(4);
        String value; //IntList value;
        Sql::extractValue<String>(&value, stmt, i); //IntList
        value = value.chop(1);
        value = value.substr(1);
        std::vector<String> value_list;
        IntList il = ListUtils::create<int>(value, ',');
        subordinate.setMetaValue(column_name, il);
        continue;
      }
      else if (column_type == DataValue::DOUBLE_LIST)
      {
        column_name = column_name.substr(4);
        String value; //DoubleList value;
        Sql::extractValue<String>(&value, stmt, i); //DoubleList
        value = value.chop(1);
        value = value.substr(1);          
        DoubleList dl = ListUtils::create<double>(value, ',');
        subordinate.setMetaValue(column_name, dl);
        continue;
      } 
      else if (column_type == DataValue::EMPTY_VALUE)
      {
        String value;
        Sql::extractValue<String>(&value, stmt, i);
        continue;
      }
    }

    if (has_bbox)
    {
      // current_subordinate add bbox
      column_nr = cols_features + cols_subordinates + 1; //+ 1 = REF_ID
      ConvexHull2D hull = readBBox_(stmt, column_nr);
      subordinate.getConvexHulls().push_back(hull);
    }

    std::cout << "subordinate id " << id_sub << std::endl;
  
    return subordinate;
  }


  int getColumnCount(sqlite3 *db, sqlite3_stmt *stmt, const String sql)
  {
    SqliteConnector::prepareStatement(db, &stmt, sql);
    sqlite3_step(stmt);
    int column_count = sqlite3_column_count(stmt);
    sqlite3_finalize(stmt);

    return column_count;
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //                                                   read FeatureMap as SQL database                                                    //
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // read SQL database and store as FeatureMap
  // fitted snippet of FeatureXMLFile::load
  // based on TransitionPQPFile::readPQPInput

  FeatureMap FeatureSQLFile::read(const std::string& filename) const
  {
    FeatureMap feature_map; // FeatureMap object as feature container

    sqlite3 *db;
    sqlite3_stmt * stmt;
    std::string select_sql;

    SqliteConnector conn(filename); // Open database
    db = conn.getDB();

    // set switches to access only existent tables
    bool features_switch = SqliteConnector::tableExists(db, "FEATURES_TABLE");
    bool subordinates_switch = SqliteConnector::tableExists(db, "FEATURES_SUBORDINATES");
    bool dataprocessing_switch = SqliteConnector::tableExists(db, "FEATURES_DATAPROCESSING");
    bool features_bbox_switch = SqliteConnector::tableExists(db, "FEATURES_TABLE_BOUNDINGBOX");
    bool subordinates_bbox_switch = SqliteConnector::tableExists(db, "SUBORDINATES_TABLE_BOUNDINGBOX");

    //////////////////////////////////////////////////////////////////////////////////////////
    // store sqlite3 database as FeatureMap                                                 //
    // 1. features                                                                          //                                    
    // 2. subordinates                                                                      //
    // 3. boundingbox                                                                       //
    // 4. dataprocessing                                                                    //
    //////////////////////////////////////////////////////////////////////////////////////////
    
    String sql;

    int cols_features = 0;
    int cols_subordinates = 0;
    int cols_features_bbox = 0;
    int cols_subordinates_bbox = 0;

    // check existence of tables in database
    // fit queries according to present tables
    // get number of columns for each table except dataprocessing

    //DEBUG
    std::cout << "\n############################ here is read ####################  \n\n\n" << std::endl;
    //////////////////////////////////////////////////////////////////////////////////////////
    // 1. features
    //////////////////////////////////////////////////////////////////////////////////////////
    if (features_switch)
    {
      sql = "SELECT * FROM FEATURES_TABLE;";
      cols_features = getColumnCount(db, stmt, sql);
      std::cout << "Number of columns for cols_features = " << cols_features << std::endl;
    }
    else if (!features_switch)
    {
      std::cout << "NO FEATURES" << std::endl;
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    // 2. subordinates
    //////////////////////////////////////////////////////////////////////////////////////////
    if (subordinates_switch)
    {
      sql = "SELECT * FROM FEATURES_SUBORDINATES;";
      cols_subordinates = getColumnCount(db, stmt, sql);
      std::cout << "Number of columns for cols_subordinates = " << cols_subordinates<< std::endl;
    }
    else if (!subordinates_switch)
    {
      std::cout << "NO SUBORDINATES" << std::endl;
    }
  
    //////////////////////////////////////////////////////////////////////////////////////////
    // 3. features bbox
    //////////////////////////////////////////////////////////////////////////////////////////
    if (features_bbox_switch)
    {
      sql = "SELECT * FROM FEATURES_TABLE_BOUNDINGBOX;";
      cols_features_bbox = getColumnCount(db, stmt, sql);
      std::cout << "Number of columns for cols_feature_bbox = " << cols_features_bbox << std::endl;
    }
    else if (!features_bbox_switch)
    {
      std::cout << "NO BBOXES OF FEATURES" << std::endl;
    }


    //////////////////////////////////////////////////////////////////////////////////////////
    // 4. subordinates bbox
    //////////////////////////////////////////////////////////////////////////////////////////
    if (subordinates_bbox_switch)
    {
      sql = "SELECT * FROM SUBORDINATES_TABLE_BOUNDINGBOX;";
      cols_subordinates_bbox = getColumnCount(db, stmt, sql);
      std::cout << "Number of columns for cols_subodinate_bbox = " << cols_subordinates_bbox << std::endl;
    }
    else if (!subordinates_bbox_switch)
    {
      std::cout << "NO BBOXES OF SUBORDINATES" << std::endl;
    }
    
    
    //////////////////////////////////////////////////////////////////////////////////////////
    // 5. concatenation features bbox
    //////////////////////////////////////////////////////////////////////////////////////////
    String features_sql = "SELECT * FROM  FEATURES_TABLE \
                            LEFT JOIN FEATURES_TABLE_BOUNDINGBOX ON FEATURES_TABLE.id = FEATURES_TABLE_BOUNDINGBOX.ref_id \
                            ORDER BY FEATURES_TABLE.ID, FEATURES_TABLE_BOUNDINGBOX.BB_IDX;";
    int cols_features_join_bbox = getColumnCount(db, stmt, sql);
    std::cout << "Number of columns of feature join bbox = " << cols_features_join_bbox << std::endl;

    // 6. concatenation features subordinates bbox 
    //sql = "SELECT * FROM  FEATURES_TABLE LEFT JOIN FEATURES_SUBORDINATES ON FEATURES_TABLE.id = FEATURES_SUBORDINATES.ref_id LEFT JOIN SUBORDINATES_TABLE_BOUNDINGBOX ON FEATURES_SUBORDINATES.id = SUBORDINATES_TABLE_BOUNDINGBOX.id ORDER BY FEATURES_TABLE.ID;";
    
    String subordinates_sql = "SELECT * FROM  FEATURES_TABLE LEFT JOIN FEATURES_SUBORDINATES ON FEATURES_TABLE.id = FEATURES_SUBORDINATES.ref_id \
                                LEFT JOIN SUBORDINATES_TABLE_BOUNDINGBOX ON FEATURES_SUBORDINATES.id = SUBORDINATES_TABLE_BOUNDINGBOX.id \
                                ORDER BY FEATURES_TABLE.ID, FEATURES_SUBORDINATES.sub_idx,SUBORDINATES_TABLE_BOUNDINGBOX.bb_idx;";
    int cols_features_join_subordinates_join_bbox = getColumnCount(db, stmt, sql);
    std::cout << "Number of columns of subordinate join bbox = " << cols_features_join_subordinates_join_bbox << std::endl;

    //////////////////////////////////////////////////////////////////////////////////////////
    // check cases of database content
    // 1. only features
    // 2. features and boundingbox
    // 3. features and subordinates
    //////////////////////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////////////////////
    // CASES
    //////////////////////////////////////////////////////////////////////////////////////////
  /*
    if (features_switch)
    {
      features_sql = "SELECT * FROM  FEATURES_TABLE";
    } 
    else if (subordinates_switch)
    {
      subordinates_sql = "SELECT * FROM  FEATURES_TABLE LEFT JOIN FEATURES_SUBORDINATES ON FEATURES_TABLE.id = FEATURES_SUBORDINATES.ref_id \
                                ORDER BY FEATURES_TABLE.ID, FEATURES_SUBORDINATES.sub_idx;";
    }
    else if (features_bbox_switch)
    {
      features_sql = "SELECT * FROM  FEATURES_TABLE \
                            LEFT JOIN FEATURES_TABLE_BOUNDINGBOX ON FEATURES_TABLE.id = FEATURES_TABLE_BOUNDINGBOX.ref_id \
                            ORDER BY FEATURES_TABLE.ID, FEATURES_TABLE_BOUNDINGBOX.BB_IDX;";
    
      subordinates_sql = "SELECT * FROM  FEATURES_TABLE LEFT JOIN FEATURES_SUBORDINATES ON FEATURES_TABLE.id = FEATURES_SUBORDINATES.ref_id \
                                LEFT JOIN SUBORDINATES_TABLE_BOUNDINGBOX ON FEATURES_SUBORDINATES.id = SUBORDINATES_TABLE_BOUNDINGBOX.id \
                                ORDER BY FEATURES_TABLE.ID, FEATURES_SUBORDINATES.sub_idx,SUBORDINATES_TABLE_BOUNDINGBOX.bb_idx;";
    }
  */




    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                                  sqlite3 database readout into FeatureMap                                            //
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    // access table entries and insert into FeatureMap feature_map 
    // begin dataprocessing #####################################
    if (dataprocessing_switch)
    {
      /// 1. get feature data from database
      String sql = "SELECT * FROM  FEATURES_DATAPROCESSING;";
      SqliteConnector::prepareStatement(db, &stmt, sql);
      //SqliteConnector::executePreparedStatement(db, &stmt, sql);
      sqlite3_step(stmt);

      while (sqlite3_column_type( stmt, 0 ) != SQLITE_NULL)
      {

        // set dataprocessing parameters
        String id_s;
        size_t id = 0;

        // extract as String TODO
        Sql::extractValue<String>(&id_s, stmt, 0);
        std::istringstream iss(id_s);
        iss >> id;

        // String id_s;
        // int64_t id = 0;
        // Sql::extractValue<String>(&id_s, stmt, 0);
        // std::istringstream iss(id_s);
        // iss >> id;


        String software;
        Sql::extractValue<String>(&software, stmt, 1);
        String software_name;
        Sql::extractValue<String>(&software_name, stmt, 2);
        String data;
        Sql::extractValue<String>(&data, stmt, 3);
        String time;
        Sql::extractValue<String>(&time, stmt, 4);
        String action;
        Sql::extractValue<String>(&action, stmt, 5);

        // get values
        // id, RT, MZ, Intensity, Charge, Quality
        // save SQL column elements as feature
        feature_map.setUniqueId(id);
        DataProcessing dp;

        //software
        dp.getSoftware().setName(software);
        dp.getSoftware().setVersion(software_name);

        //time
        DateTime date_time;
        date_time.set(data + " " + time);
        dp.setCompletionTime(date_time);

        //actions
        std::set<DataProcessing::ProcessingAction> proc_actions;

        for (int i = 0; i < DataProcessing::SIZE_OF_PROCESSINGACTION; ++i)
        {
          if (action == DataProcessing::NamesOfProcessingAction[i])
          {
            proc_actions.insert((DataProcessing::ProcessingAction) i);
          }
        }
        dp.setProcessingActions(proc_actions);

        // access number of columns and infer DataType
        int cols = sqlite3_column_count(stmt);
        int column_type = 0;

        String column_name;

        for (int i = 5; i < cols ; ++i)
        {
          String column_name = sqlite3_column_name(stmt, i);
          column_type = getColumnDatatype(column_name);

          if (sqlite3_column_type(stmt,i) == SQLITE_NULL)
          {
            break;
          }
          else if (column_type == DataValue::STRING_VALUE)
          {
            String value;
            column_name = column_name.substr(3);
            Sql::extractValue<String>(&value, stmt, i);
            dp.setMetaValue(column_name, value); 
            continue;            
          } 
          else if (column_type == DataValue::INT_VALUE)
          {
            int value = 0;
            column_name = column_name.substr(3);
            Sql::extractValue<int>(&value, stmt, i);
            dp.setMetaValue(column_name, value); 
            continue;            
          }
          else if (column_type == DataValue::DOUBLE_VALUE)
          {
            double value = 0.0;
            column_name = column_name.substr(3);
            Sql::extractValue<double>(&value, stmt, i);          
            dp.setMetaValue(column_name, value); 
            continue;            
          }
          else if (column_type == DataValue::STRING_LIST)
          {
            String value; 
            Sql::extractValue<String>(&value, stmt, i);
            column_name = column_name.substr(4);
            StringList sl;
            // cut off "[" and "]""
            value = value.chop(1);
            value = value.substr(1);
            value.split(", ", sl);
            dp.setMetaValue(column_name, value); 
            continue;            
          }
          else if (column_type == DataValue::INT_LIST)
          {
            String value; //IntList value;
            Sql::extractValue<String>(&value, stmt, i); //IntList
            column_name = column_name.substr(4);
            value = value.chop(1);
            value = value.substr(1);
            std::vector<String> value_list;
            IntList il = ListUtils::create<int>(value, ',');
            dp.setMetaValue(column_name, value); 
            continue;            
          }
          else if (column_type == DataValue::DOUBLE_LIST)
          {
            String value; //DoubleList value;
            Sql::extractValue<String>(&value, stmt, i); //DoubleList
            column_name = column_name.substr(4);
            value = value.chop(1);
            value = value.substr(1);          
            DoubleList dl = ListUtils::create<double>(value, ',');
            dp.setMetaValue(column_name, value); 
            continue;            
          }
          else if (column_type == DataValue::EMPTY_VALUE)
          {
            String value;
            column_name = column_name.substr(3);
            Sql::extractValue<String>(&value, stmt, i);
            dp.setMetaValue(column_name, value); 
            continue;            
          }
        }

        feature_map.getDataProcessing().push_back(dp);
        sqlite3_step(stmt);

      }

      sqlite3_finalize(stmt);

    } // end of dataprocessing ####################################


    std::map<int64_t, size_t> map_fid_to_index; // map feature ids as indices in map 

    std::cout << "sql = \t " << sql  << std::endl; 
    std::cout << "features_sql = \t" << features_sql << std::endl; 
    std::cout << "subordinates_sql = \t" << subordinates_sql << std::endl; 


    // begin features #############################################
    if (features_switch)
    {
      /// 1. get feature data from database
      SqliteConnector::prepareStatement(db, &stmt, features_sql);
      sqlite3_step(stmt);
      
      Feature* feature = nullptr;
      bool has_bbox = false;

      // set f_id_prev to 0 to ensure that if an id is read 
      // a new entry in the feature_map can be generated
      int64_t f_id_prev = 0;            
      int counter = 0;

      while (sqlite3_column_type( stmt, 0 ) != SQLITE_NULL)
      {
        int64_t f_id = 0;
        String id_str;
        // return value of feature ID in column #0
        Sql::extractValue<String>(&id_str, stmt, 0);    
        std::istringstream iss(id_str);
        iss >> f_id;

        int64_t ref_id = 0;
        String ref_id_str;
        // return position of REF_ID to check for bbox entries
        Sql::extractValue<String>(&ref_id_str, stmt, cols_features);
        std::istringstream issref(ref_id_str);
        issref >> ref_id;

        int bbox_idx = 0;
        if (ref_id == 0)
        {
          has_bbox = false; // no bbox, ref_id is 0
        } 
        else if (ref_id != 0)
        {
          has_bbox = true;
          // return position of last entry in query
          Sql::extractValue<int>(&bbox_idx, stmt, (cols_features + cols_features_bbox - 1));          
        }

        if (f_id != f_id_prev)
        {
          feature_map.push_back(Feature());
          f_id_prev = f_id; // set previous feature id 
        }

        // push back if f_id != f_id_prev

        feature = &feature_map[feature_map.size() - 1];

        // get values id, RT, MZ, Intensity, Charge, Quality



        std::cout << counter << "nth feature with id: " << f_id << std::endl;
        //int ref_id = 0;
        //Sql::extractValue<int>(&ref_id, stmt, 56);
        ++counter;

        double rt = 0.0;
        Sql::extractValue<double>(&rt, stmt, 1);
        double mz = 0.0;
        Sql::extractValue<double>(&mz, stmt, 2);
        double intensity = 0.0;
        Sql::extractValue<double>(&intensity, stmt, 3);
        int charge = 0;
        Sql::extractValue<int>(&charge, stmt, 4);
        double quality = 0.0;
        Sql::extractValue<double>(&quality, stmt, 5);

        feature->setUniqueId(f_id);
        feature->setRT(rt);
        feature->setMZ(mz);
        feature->setIntensity(intensity);
        feature->setCharge(charge);
        feature->setOverallQuality(quality);

        int col_nr = 6; // index to account for offsets in joined tables
        for (int i = col_nr; i < cols_features ; ++i)  // save userparam columns
        {
          String column_name = sqlite3_column_name(stmt, i);
          int column_type = getColumnDatatype(column_name);
          
          // test if database column value is NULL
          if (sqlite3_column_type(stmt,i) == SQLITE_NULL)
          {
            break;
          }
          else if (column_type == DataValue::STRING_VALUE)
          {
            column_name = column_name.substr(3);
            String value;
            Sql::extractValue<String>(&value, stmt, i);
            feature->setMetaValue(column_name, value); 
            continue;
          } 
          else if (column_type == DataValue::INT_VALUE)
          {
            column_name = column_name.substr(3);
            int value = 0;
            Sql::extractValue<int>(&value, stmt, i);
            feature->setMetaValue(column_name, value); 
            continue;
          } 
          else if (column_type == DataValue::DOUBLE_VALUE)
          {
            column_name = column_name.substr(3);
            double value = 0.0;
            Sql::extractValue<double>(&value, stmt, i);          
            feature->setMetaValue(column_name, value); 
            continue;
          } 
          else if (column_type == DataValue::STRING_LIST)
          {
            column_name = column_name.substr(4);
            String value; 
            Sql::extractValue<String>(&value, stmt, i);

            StringList sl;
            // cut off "[" and "]""
            value = value.chop(1);
            value = value.substr(1);
            value.split(", ", sl);
            feature->setMetaValue(column_name, sl);
            continue;
          } 
          else if (column_type == DataValue::INT_LIST)
          {
            column_name = column_name.substr(4);
            String value; //IntList value;
            Sql::extractValue<String>(&value, stmt, i); //IntList
            value = value.chop(1);
            value = value.substr(1);
            std::vector<String> value_list;
            IntList il = ListUtils::create<int>(value, ',');
            feature->setMetaValue(column_name, il);
            continue;
          } 
          else if (column_type == DataValue::DOUBLE_LIST)
          {
            column_name = column_name.substr(4);
            String value; //DoubleList value;
            Sql::extractValue<String>(&value, stmt, i); //DoubleList
            value = value.chop(1);
            value = value.substr(1);          
            DoubleList dl = ListUtils::create<double>(value, ',');
            feature->setMetaValue(column_name, dl);
            continue;
          } 
          else if (column_type == DataValue::EMPTY_VALUE)
          {
            //std::cout << " else if (column_type == DataValue::EMPTY_VALUE " << column_type << std::endl;
            break;
            //String value;
            //Sql::extractValue<String>(&value, stmt, i);
            //continue;
          }
        }
        
        if (has_bbox == false) // add  bbox to feature if present
        {
          map_fid_to_index[feature->getUniqueId()] = feature_map.size() - 1; 
        }
        else if (has_bbox == true)
        {
          if (bbox_idx == 0)
          {
            ConvexHull2D hull = readBBox_(stmt, cols_features);
            feature->getConvexHulls().push_back(hull);
            map_fid_to_index[feature->getUniqueId()] = feature_map.size() - 1; 
          }
          else if (bbox_idx > 0) // add new bb to existing feature
          {
            ConvexHull2D hull = readBBox_(stmt, cols_features);
            feature->getConvexHulls().push_back(hull);
          }
        }
      
        sqlite3_step(stmt);  
      }
      sqlite3_finalize(stmt);
    } // end of features ##########################################


    for (std::map<int64_t, size_t>::iterator it = map_fid_to_index.begin(); it != map_fid_to_index.end(); ++it)
    {
      std::cout << "Value at " <<  it->first << " is " << it->second << std::endl;
    }


    // get subordinate entries #########################################
    if (subordinates_switch)
    {
      SqliteConnector::prepareStatement(db, &stmt, subordinates_sql);
      sqlite3_step(stmt);
      int column_nr = 0;
      int64_t f_id_prev = 0;
      int64_t sub_id_prev = 0;
      bool has_bbox = false;
      int bbox_idx = 0;

      int64_t f_id = 0; // get feature id entries for feature- and userparameters
      String f_id_string;

      int64_t sub_id = 0; // get subordinate id entries for feature- and userparameters
      String sub_id_string;

      int64_t cvhull_id = 0; // get bbox id entries of convex hulls by subordinate-> boundingboxes
      String cvhull_id_string;

      while (sqlite3_column_type( stmt, 0 ) != SQLITE_NULL)       // while joined table still has entries continue 
      {
        Sql::extractValue<String>(&f_id_string, stmt, 0);
        std::istringstream issf(f_id_string);
        issf >> f_id;

        // extract ref_id
        Sql::extractValue<String>(&sub_id_string, stmt, (cols_features + cols_subordinates));
        std::istringstream issub(sub_id_string);
        issub >> sub_id;
        std::cout << "sub_id " << sub_id << std::endl;

        // try last entry (bbox id?)
        Sql::extractValue<String>(&cvhull_id_string, stmt, (cols_features + cols_subordinates + cols_subordinates_bbox - 1));
        std::istringstream issref(cvhull_id_string);
        issref >> cvhull_id;
        std::cout << "cvhull_id " << cvhull_id << std::endl;


        // test presence of convex hulls 
        if (cvhull_id == 0)  // 0 as impossible value for UniqueIdinterface, cast NULL to 0 via extractValue()
        {
          has_bbox = false; // no bbox, cvhull_id is 0
          //cout << "\ncvhull_id is " << cvhull_id << endl;
        } 
        else if (cvhull_id != 0) // value of cvhull_id is not NULL and fits the UniqueIdInterface
        {
          has_bbox = true;  // bbox is present
          Sql::extractValue<int>(&bbox_idx, stmt, 79); // last column with subordinate bbox_idx of joined table          
          //cout << "\ncvhull_id " << cvhull_id << " has bbox_idx with " << bbox_idx << " as value." << endl;
        }


        Feature* feature = &feature_map[map_fid_to_index[f_id]];  // get index of feature vector subordinates at index of id
        std::vector<Feature>* subordinates = &feature->getSubordinates(); 





        std::cout << "\nsub_id: " << sub_id << std::endl; // show subordinate id
        std::cout << "cvhull_id: " << cvhull_id << std::endl; // show subordinate id

        // break current query if subordinate entry is 0, case is possible        
        if (sub_id == 0)
        {
          std::cout << "no subordinate" << std::endl;
          sqlite3_step(stmt);
          continue;
        } 

        if (f_id != f_id_prev) // new feature -> presumes new cvhull_id has bbox with bbox_idx being 0
        {
          std::cout << "f_id != f_id_prev; f_id: " << f_id << " f_id_prev: " << f_id_prev << std::endl;
          std::cout << "new feature and new subordinate id " << sub_id << std::endl;
          f_id_prev = f_id; // set previous to current feature for subordinate storage
          sub_id_prev = sub_id;
      
          column_nr = cols_features + 1 + 1;  // size of features column + column SUB_IDX + column REF_ID
          if (cvhull_id == 0)
          {
            std::cout << "... but no convexhull" << std::endl;
            subordinates->push_back(readSubordinate_(stmt, column_nr, cols_features, cols_subordinates, has_bbox)); // read subordinate w/o bbox
          }
          else if (cvhull_id != 0)
          {
            std::cout << "...  and convexhull" << std::endl;
            subordinates->push_back(readSubordinate_(stmt, column_nr, cols_features, cols_subordinates, has_bbox)); // read subordinate with first bounding box
          }

        }
        else // same feature
        {   
          std::cout << "f_id == f_id_prev; f_id: " << f_id << " f_id_prev: " << f_id_prev << std::endl;
          std::cout << "same feature with f_id  " << f_id << ", sub_id " << sub_id << " and sub_id_prev " << sub_id_prev  << std::endl;
          
          //  test if subordinate is equal to previous subordinate
            
          
          if (sub_id_prev == sub_id)
          {
            std::cout << " same subordinate, add bbox to its convexhull " << sub_id << std::endl;
            (*subordinates)[subordinates->size()-1].getConvexHulls().push_back(readBBox_(stmt, column_nr));
          }
          else if (sub_id_prev != sub_id)
          {
            std::cout << " different subordinate, push back to subordinates " << sub_id << std::endl;
            subordinates->push_back(readSubordinate_(stmt, column_nr, cols_features, cols_subordinates, has_bbox)); // read subordinate with first bounding box

          }











        }

        sqlite3_step(stmt);
      } // end of while of subordinate entries
      sqlite3_finalize(stmt);
    } // end of subordinates ############################################
  

    return feature_map;
  }  // end of FeatureSQLFile::read
} // namespace OpenMS
