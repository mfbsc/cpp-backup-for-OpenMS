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
#include <sqlite3.h>
#include <sstream>

#include <iostream>


#include <OpenMS/FORMAT/IdXMLFile.h>
#include <OpenMS/METADATA/DataProcessing.h>
#include <OpenMS/DATASTRUCTURES/DateTime.h>
#include <OpenMS/DATASTRUCTURES/Param.h>
#include <OpenMS/CHEMISTRY/ProteaseDB.h>
#include <OpenMS/FORMAT/FileHandler.h>



namespace OpenMS
{
  /*
  FeatureSQLFile::FeatureSQLFile()
  {
    
  }

  FeatureSQLFile::~FeatureSQLFile()
  {

  }
  */

  vector<String>& FeatureSQLFile::getDuplicateLabels(const std::vector<String>& feature_elements, std::vector<String>& userparam_elements)
  {
    std::vector<String> sorted_feature_v = std::sort(feature_elements.begin(), feature_elements.end())
    std::vector<String> sorted_userparam_v = std::sort(userparam_elements.begin(), userparam_elements.end())
    std::vector<String> v_intersection;

    std::set_intersection(sorted_feature_v.begin(), sorted_feature_v.end(), sorted_userparam_v.begin(), sorted_userparam_v.end(), std::back_inserter(v_intersection));

    for (const string& elem : v_intersection)
    {
      if (v_intersection == sorted_userparam_v[elem])
      {
        sorted_userparam_v[elem] == sorted_userparam_v[elem].insert(0, 1, '_');
      }
    } 
    
    return sorted_userparams_v;
  }

  vector<String>& FeatureSQLFile::prefixLabel(vector<String> types of header)
  {
    // return vector label, vector column_type
  }

  vector<String> FeatureSQLFile::formatSQLEntries()
  {
    std::vector<String> column_label_v;
    std::vector<String> column_type_v;
    
    for ()

  }






  // read SQL database and store as FeatureMap
  FeatureMap FeatureSQLFile::read(const std::string& filename) const
  {
    if (!File::exists(filename))
    {
      throw Exception::FileNotFound(__FILE__, __LINE__, OPENMS_PRETTY_FUNCTION, filename);
    }

    if (!File::readable(filename))
    {
      throw Exception::FileNotReadable(__FILE__, __LINE__, OPENMS_PRETTY_FUNCTION, filename);
    }

    if (infile_.is_open()) infile_.close(); // precaution

    infile_.open(filename.c_str(), std::ios::binary | std::ios::in);


  }

  // store FeatureXML as SQL database
  void FeatureSQLFile::write(const std::string& out_fm, const FeatureMap& fm) const
  {
    
  }

  /*
  void FeatureSQLFile::read(const std::string& in_xml)
    {
      sqlite3_stmt * stmt;
      std::string select_sql;

      // Open database
      SqliteConnector conn(in_xml);

      if ()
      {
        select_sql = "SELECT * FROM ".append(table_name)
      }
      else if
      {
        std::cout << 
      }

      // Execute SQL select statement
      conn.executePreparedStatement(&stmt, select_sql);
      sqlite3_step( stmt );

      int cols = sqlite3_column_count(stmt);

      // Generate features
      int k = 0;
      std::vector<std::string> group_id_index;

      while (sqlite3_column_type( stmt, 0 ) != SQLITE_NULL)
      {
        int label = 0;
        std::map<std::string, double> features;

        for (int i = 0; i < cols; i++)
        {
          if (OpenMS::String(sqlite3_column_name( stmt, i )) == "FEATURE_ID")
          {
            psm_id = OpenMS::String(reinterpret_cast<const char*>(sqlite3_column_text( stmt, i )));
          }
          if (OpenMS::String(sqlite3_column_name( stmt, i )) == "GROUP_ID")
          {
            if (std::find(group_id_index.begin(), group_id_index.end(),
                  OpenMS::String(reinterpret_cast<const char*>(sqlite3_column_text( stmt, i )))) != group_id_index.end())
            {
              scan_id = std::find(group_id_index.begin(), group_id_index.end(),
                          OpenMS::String(reinterpret_cast<const char*>(sqlite3_column_text( stmt, i )))) - group_id_index.begin();
            }
            else
            {
              group_id_index.push_back(OpenMS::String(reinterpret_cast<const char*>(sqlite3_column_text( stmt, i ))));
              scan_id = std::find(group_id_index.begin(), group_id_index.end(),
                          OpenMS::String(reinterpret_cast<const char*>(sqlite3_column_text( stmt, i )))) - group_id_index.begin();
            }
          }
          if (OpenMS::String(sqlite3_column_name( stmt, i )) == "DECOY")
          {
            if (sqlite3_column_int( stmt, i ) == 1)
            {
              label = -1;
            }
            else
            {
              label = 1;
            }
          }
          if (OpenMS::String(sqlite3_column_name( stmt, i )) == "MODIFIED_SEQUENCE")
          {
            peptide = OpenMS::String(reinterpret_cast<const char*>(sqlite3_column_text( stmt, i )));
          }
          if (OpenMS::String(sqlite3_column_name( stmt, i )).substr(0,4) == "VAR_")
          {
            features[OpenMS::String(sqlite3_column_name( stmt, i ))] = sqlite3_column_double( stmt, i );
          }
        }

        // Write output
        if (k == 0)
        {
          pin_output << "PSMId\tLabel\tScanNr";
          for (auto const &feat : features)
          {
            pin_output << "\t" << feat.first;
          }
          pin_output << "\tPeptide\tProteins\n";
        }
        pin_output << psm_id << "\t" << label << "\t" << scan_id;
        for (auto const &feat : features)
        {
          pin_output << "\t" << feat.second;
        }
        pin_output << "\t." << peptide << ".\tProt1" << "\n";

        sqlite3_step( stmt );
        k++;
      }

      sqlite3_finalize(stmt);

      if (k==0)
      {
        if (osw_level == "transition")
        {
        }
        else
        {
          throw Exception::FileEmpty(__FILE__, __LINE__, __FUNCTION__, in_osw);
        }
      }

    }



    void FeatureSQLFile::write(const std::string& in_osw,const std::map< std::string, std::vector<double> >& features)
    {
      std::string table;
      std::string create_sql;

      if (osw_level == "")
      {
        table_dataproc = "dataproc";
        create_sql =  "DROP TABLE IF EXISTS " << table_dataproc << "; " \
                      "CREATE TABLE " << table_dataproc << "(" \
                      "PEP DOUBLE NOT NULL);";
        table_subordinates = "subordinates";
        create_sql =  "DROP TABLE IF EXISTS " << table_subordinates << "; " \
                      "CREATE TABLE " << table_subordinates << "(" \
        table_features = "features";
        create_sql =  "DROP TABLE IF EXISTS " << table_features << "; " \
                      "CREATE TABLE " << table_features << "(" \


      std::vector<std::string> insert_sqls;
      for (auto const &feat : features)
      {
        std::stringstream insert_sql;
        if (osw_level == "transition") {
          std::vector<OpenMS::String> ids;
          OpenMS::String(feat.first).split("_", ids);
          insert_sql << "INSERT INTO " << table;
          insert_sql << " (FEATURE_ID, TRANSITION_ID, SCORE, QVALUE, PEP) VALUES (";
          insert_sql <<  ids[0] << ",";
          insert_sql <<  ids[1] << ",";
          insert_sql <<  feat.second[0] << ",";
          insert_sql <<  feat.second[1] << ",";
          insert_sql <<  feat.second[2] << "); ";
        }
        else
        {
          insert_sql << "INSERT INTO " << table;
          insert_sql << " (FEATURE_ID, SCORE, QVALUE, PEP) VALUES (";
          insert_sql <<  feat.first << ",";
          insert_sql <<  feat.second[0] << ",";
          insert_sql <<  feat.second[1] << ",";
          insert_sql <<  feat.second[2] << "); ";
        }

        insert_sqls.push_back(insert_sql.str());
      }

      // Write to Sqlite database
      SqliteConnector conn(in_osw);
      conn.executeStatement(create_sql);
      conn.executeStatement("BEGIN TRANSACTION");
      for (size_t i = 0; i < insert_sqls.size(); i++)
      {
        conn.executeStatement(insert_sqls[i]);
      }
      conn.executeStatement("END TRANSACTION");
    }




  */
} // namespace OpenMS



/*

// try to establish database connection
SqliteConnector get_DB_connection(const std::string& db_filename)
{
  
}

datatype readTable(SqliteConnector connection, const std::string table_name)
{

}

datatype getHeader(table)
{

}

*/