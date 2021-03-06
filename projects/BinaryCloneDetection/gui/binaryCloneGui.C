#include "rose.h"
#include <boost/algorithm/string.hpp>
#include "binaryCloneGui.h"
#include <ext/hash_map>


#include <sstream>
#include <QFileDialog>

#include <unistd.h>

#include "boost/filesystem/operations.hpp" // includes boost/filesystem/path.hpp
#include <boost/lexical_cast.hpp>
#include <iostream>

#include "icons.h"

//#include "disks.xpm"
#include "folder.xpm"

#define EMACS

using namespace qrs;
using namespace boost::filesystem;
using namespace std;
using namespace boost;
using namespace sqlite3x;
using namespace __gnu_cxx;

static string htmlEscape(const string& s) {
  string s2;
  for (size_t i = 0; i < s.size(); ++i) {
    switch (s[i]) {
      case '<': s2 += "&lt;"; break;
      case '>': s2 += "&gt;"; break;
      case '&': s2 += "&amp;"; break;
      default: s2 += s[i]; break;
    }
  }
  return s2;
}
class DeleteAST : public SgSimpleProcessing
   {
     public:
      //! Required traversal function
          void visit (SgNode* node);
   };

void
DeleteAST::visit(SgNode* node)
   {
     delete node;
   }

void DeleteSgTree( SgNode* root)
{
  DeleteAST deleteTree;
  deleteTree.traverse(root,postorder);
}




void toolbarClick(int action)
{
  BinaryCloneGui *instance = QROSE::cbData<BinaryCloneGui *>();

  switch(action){
    case 0:
      instance->run();
      break;
 
    case 2: 		
      instance->open();     
      break;
    case 3: 		
      QROSE::exit(0); break;
      break;

    default:
      eAssert(0, ("internal error: invalid selection type!"));

  }
  
}


static void tableCellActivated(int col, int row, int oldCol, int oldRow) 
{

  BinaryCloneGui *instance = QROSE::cbData<BinaryCloneGui *>();
  instance->unhighlightRow(oldRow);
  instance->highlightRow(row);

  return;

} //tableCellActivated(int col, int row, int oldCol, int oldRow)


static void viewBoxActivated(int selection) 
{

  BinaryCloneGui *instance = QROSE::cbData<BinaryCloneGui *>();
  instance->selectView(selection);

  return;
}


static void selectLockOrUnlockBars(int selection) 
{

  BinaryCloneGui *instance = QROSE::cbData<BinaryCloneGui *>();
  instance->lockBars(selection);

  return;
}


void
BinaryCloneGui::selectView(int selection)
{

  switch (selection) {
    case 0:
      codeWidget->setHtml(QString(unparsedView.first.c_str()));
      codeWidget2->setHtml(QString(unparsedView.second.c_str()));
      break;
    case 1:
      codeWidget->setHtml(QString(normalizedView.first.c_str()));
      codeWidget2->setHtml(QString(normalizedView.second.c_str()));
      break;
    case 2:
      codeWidget->setHtml(QString(allInsnsUnparsedView.first.c_str()));
      codeWidget2->setHtml(QString(allInsnsUnparsedView.second.c_str()));
      break;

  }


};


void
BinaryCloneGui::lockBars(int selection)
{
  switch(selection)
  {
    case 0:
      QROSE::disconnect_ex((const QObject*)(codeWidget->verticalScrollBar()), SIGNAL(valueChanged(int)),
          (const QObject*)(codeWidget2->verticalScrollBar()), SLOT(setValue(int)));
      QROSE::disconnect_ex((const QObject*)(codeWidget2->verticalScrollBar()), SIGNAL(valueChanged(int)),
          (const QObject*)(codeWidget->verticalScrollBar()), SLOT(setValue(int)));

      break;
    case 1:
      QROSE::connect_ex((const QObject*)(codeWidget->verticalScrollBar()), SIGNAL(valueChanged(int)),
          (const QObject*)(codeWidget2->verticalScrollBar()), SLOT(setValue(int)));
      QROSE::connect_ex((const QObject*)(codeWidget2->verticalScrollBar()), SIGNAL(valueChanged(int)),
          (const QObject*)(codeWidget->verticalScrollBar()), SLOT(setValue(int)));

      break;

  }

}


BinaryCloneGui::BinaryCloneGui( ) :
  window(0)
{
  window = new QRWindow( "mainWindow", QROSE::TopDown );

  {
    //--------------------------------------------------------------------------

    QRToolBar *toolbar = (*window)["toolbar"] << new QRToolBar(QROSE::LeftRight, true, true, true);

    // icons are defined in <icons.h>
    toolbar->addButton("run");   toolbar->setPicture(0, iconRun); 
    toolbar->addButton("reset"); toolbar->setPicture(1, iconReset);
    toolbar->addSeparator();
    toolbar->addButton("open"); toolbar->setPicture(2, folder_xpm);
    toolbar->addButton("quit"); toolbar->setPicture(3, iconQuit);

    QROSE::link(toolbar, SIGNAL(clicked(int)), &toolbarClick, this);

  } //window 



  QRPanel &tiledPanel = *window << *( new QRPanel(QROSE::TopDown, QROSE::UseSplitter) );
    {

       QRPanel &lowerInnerTiledPanel = tiledPanel << *new QRPanel(QROSE::LeftRight, QROSE::UseSplitter);

       {
      tableWidget = lowerInnerTiledPanel << new QRTable( 8, "file A", "function A", "begin address", "end address", "file B", "function B", "begin address", "end address" );
      QROSE::link(tableWidget, SIGNAL(activated(int, int, int, int)), &tableCellActivated, this);


      QRPanel &parameters = lowerInnerTiledPanel << *( new QRPanel(QROSE::TopDown, QROSE::UseSplitter) );

      QGroupBox *selectGroup =  parameters <<  new QGroupBox(("Data Selection Parameters"));
      {


        QGridLayout *echoLayout =  new QGridLayout;
        QLabel *echoLabel = new QLabel("File:");
                echoLayout->addWidget(echoLabel, 0, 0);
        fileDataRestriction = new QLineEdit;
        echoLayout->addWidget(fileDataRestriction, 0, 1);
        
        QLabel *functionLabel = new QLabel("Function:");
        echoLayout->addWidget(functionLabel, 1, 0);
        functionDataRestriction = new QLineEdit;
        echoLayout->addWidget(functionDataRestriction, 1, 1 );


       wholeFunction = new QComboBox;
       wholeFunction->addItem(("No"));
       wholeFunction->addItem(("Yes"));

       QLabel *wholeFunctionLabel = new QLabel("Only whole functions:");
       echoLayout->addWidget(wholeFunctionLabel, 2, 0 );

       echoLayout->addWidget(wholeFunction, 2, 1 );

        selectGroup->setLayout(echoLayout);
      }
      
      comboBox = new QComboBox;
      comboBox->addItem(("Instruction"));
      comboBox->addItem(("Normalized"));
      comboBox->addItem(("Whole Function"));

      
      QGroupBox *echoGroup =  parameters <<  new QGroupBox(("Selection Clone-View"));
      QGridLayout *echoLayout =  new QGridLayout;
      QLabel *echoLabel = new QLabel("Views:");
      echoLayout->addWidget(echoLabel, 0, 0);
      echoLayout->addWidget(comboBox, 0, 1);


      QLabel *lockBarsLabel = new QLabel("Lock ScrollBars:");
      checkBoxLockBars = new QComboBox;
      checkBoxLockBars->addItem(("No"));
      checkBoxLockBars->addItem(("Yes"));

      echoLayout->addWidget(lockBarsLabel,1,0);
      echoLayout->addWidget(checkBoxLockBars,1,1);
      echoGroup->setLayout(echoLayout);
      

      QROSE::link(checkBoxLockBars, SIGNAL(activated(int)), &selectLockOrUnlockBars, this);
     
      
      QROSE::link(comboBox, SIGNAL(activated(int)), &viewBoxActivated, this);
      

       }

       
      
      QRPanel &upperInnerTiledPanel = tiledPanel << *new QRPanel(QROSE::LeftRight, QROSE::UseSplitter);

      {

        codeWidget = upperInnerTiledPanel << new QTextEdit;//new QREdit(QREdit::Box);
        codeWidget->setReadOnly(true);

        codeWidget2 = upperInnerTiledPanel << new QTextEdit;//new QREdit(QREdit::Box);
        codeWidget2->setReadOnly(true);

      
      } //tiledPanel
    tiledPanel.setTileSize(40,60);
  } //window 


  window->setGeometry(0,0,1280,800);
  window->setTitle("BinaryCloneMainGui");

} //BinaryCloneGui::BinaryCloneGui()

void BinaryCloneGui::open()
{
  char buf[4096] = "\0";

  std::string database = QFileDialog::getOpenFileName( 0, "Open As", getcwd(buf, 4096), "ASCII (*.sql)").toStdString();

  if( database.empty() ) return;


  if(exists(database) == true) 
  {
     con.close();
     codeWidget->setReadOnly(true);
     codeWidget->setPlainText(QString("foobar\nBar\nFoobari3"));
     con.open(database.c_str());
  }
  

} //CompassGui::open()


void
BinaryCloneGui::run( ) 
{
    codeWidget->setPlainText(QString("foobar\nBar\nFoobar2"));

    //Fill in table
    // unlink activation callback, which otherwise would trigger each
    // time we add a row in the table.
    QROSE::unlink(tableWidget, SIGNAL(activated(int, int, int, int)));

    while(tableWidget->rowCount()) tableWidget->removeRow(0);

    size_t eltCount = 0;

    try {
      eltCount = boost::lexical_cast<size_t>(con.executestring("select max(row_number) from largest_clones"));
    } catch (std::exception& e) { 
      eAssert(0, ("internal error: No largest_clones table found -- invalid database?!"));
    }

    if (eltCount == 0) {
      eAssert(0, ("internal error: No largest_clones table found -- invalid database?!"));
      exit (1);
    }

    vectorOfClones.allocate(eltCount);

    try {
      similarity = sqlite3x::sqlite3_command(con, "select similarity_threshold from detection_parameters limit 1").executedouble();
    } catch (std::exception& ex) {std::cerr << "Exception Occurred: " << ex.what() << std::endl;}
    try {
      windowSize = sqlite3x::sqlite3_command(con, "select window_size from run_parameters limit 1").executeint();
    } catch (std::exception& ex) {std::cerr << "Exception Occurred: " << ex.what() << std::endl;}
    try {
      stride = sqlite3x::sqlite3_command(con, "select stride from run_parameters limit 1").executeint();
    } catch (std::exception& ex) {std::cerr << "Exception Occurred: " << ex.what() << std::endl;}




    //Read whole dataset from database
    try{
      std::string selectSeparateDatasets ="select   function_id_A, function_id_B, begin_index_within_function_A, end_index_within_function_A, begin_index_within_function_B, end_index_within_function_B, f1.file, f1.function_name, f2.file, f2.function_name from largest_clones c join function_ids f1 on f1.row_number=c.function_id_A join function_ids f2 on f2.row_number=c.function_id_B ";


      if( wholeFunction->currentIndex() == 1)
      {
        selectSeparateDatasets += " join function_statistics stat1 on stat1.function_id=function_id_A join function_statistics stat2 on stat2.function_id=function_id_B ";

        selectSeparateDatasets += " where ( (stat1.num_instructions - " +boost::lexical_cast<string>(windowSize)+ ")/" +boost::lexical_cast<string>(stride)+") =( end_index_within_function_A-begin_index_within_function_A  )";
        selectSeparateDatasets += " AND   ( (stat2.num_instructions - " +boost::lexical_cast<string>(windowSize)+ ")/" +boost::lexical_cast<string>(stride)+") =( end_index_within_function_B-begin_index_within_function_B  )"  ;

      }
      
      if( fileDataRestriction->text() != "" || functionDataRestriction->text() != ""  )
      {
       

        if(wholeFunction->currentIndex() == 1) 
          selectSeparateDatasets += " AND ";
        else
          selectSeparateDatasets+=" where ";


        
        if( fileDataRestriction->text() != ""  )
        {
          std::string text( fileDataRestriction->text().toAscii());
          selectSeparateDatasets+= " ( f1.file like \""+ text + "\" OR f2.file like \"" + 
            text  +"\" ) ";
          
          if( functionDataRestriction->text() != ""  )
            selectSeparateDatasets+=" AND ";
        }

        if( functionDataRestriction->text() != ""  )
        {
          std::string text( functionDataRestriction->text().toAscii());
          selectSeparateDatasets+= "( f1.function_name like \""+ text + "\" OR f2.function_name like \"" + 
            text  +"\" )";
        }
      }

      std::cout << "SQL QUERY : " << selectSeparateDatasets << std::endl;
      
      sqlite3_command cmd(con, selectSeparateDatasets.c_str());
      sqlite3_reader datasets=cmd.executereader();

      int row = 0;

      while(datasets.read())
      {

        Element cur_elem;
        cur_elem.function_A = boost::lexical_cast<uint64_t>(datasets.getstring(0));
        cur_elem.function_B = boost::lexical_cast<uint64_t>(datasets.getstring(1));
        cur_elem.begin_index_within_function_A = boost::lexical_cast<uint64_t>(datasets.getstring(2));
        cur_elem.end_index_within_function_A   = boost::lexical_cast<uint64_t>(datasets.getstring(3));
        cur_elem.begin_index_within_function_B = boost::lexical_cast<uint64_t>(datasets.getstring(4));
        cur_elem.end_index_within_function_B   = boost::lexical_cast<uint64_t>(datasets.getstring(5));
        cur_elem.file_A = datasets.getstring(6);
        cur_elem.function_name_A = datasets.getstring(7);
        cur_elem.file_B = datasets.getstring(8);
        cur_elem.function_name_B = datasets.getstring(9);



        tableWidget->addRows(1);

        tableWidget->setText(boost::lexical_cast<std::string>(cur_elem.file_A), 0, row);

        tableWidget->setText(boost::lexical_cast<std::string>(cur_elem.function_name_A), 1, row);
        tableWidget->setText(boost::lexical_cast<std::string>(cur_elem.begin_index_within_function_A), 2, row);
        tableWidget->setText(boost::lexical_cast<std::string>(cur_elem.end_index_within_function_A), 3, row);
        tableWidget->setText(boost::lexical_cast<std::string>(cur_elem.file_B), 4, row);

        tableWidget->setText(boost::lexical_cast<std::string>(cur_elem.function_name_B), 5, row);
        tableWidget->setText(boost::lexical_cast<std::string>(cur_elem.begin_index_within_function_B), 6, row);
        tableWidget->setText(boost::lexical_cast<std::string>(cur_elem.end_index_within_function_B), 7, row);

        tableWidget->setHAlignment(true, false, 0); // left horizontal alignment

        vectorOfClones[row] = cur_elem;
        row++;
      }
    }
    catch(std::exception& e) {
	//cout << e.what() << "\n";
  }

    QROSE::link(tableWidget, SIGNAL(activated(int, int, int, int)), 
        &tableCellActivated, this);

}

SgNode* BinaryCloneGui::disassembleFile(std::string tsv_directory){
  SgNode* globalBlock;

  if(exists(tsv_directory) == false)
  {

    char buf[4096] = "\0";

    int i = 0; 
    while( exists ( relativePathPrefix +"/"+ tsv_directory) == false ) 
    {
      
      if(i>10){
        std::string error_message = "user error: Relative Path to  ";
        error_message+=tsv_directory;
        error_message+=" not selected in " ;
        error_message+= boost::lexical_cast<std::string>(i);
        error_message+=" attempts.";
        eAssert(0,  (error_message.c_str()) );
      }

      QFileDialog dialog;
      dialog.setFileMode(QFileDialog::DirectoryOnly);
      //relativePathPrefix = dialog.getOpenFileName( 0, "Relative Path To Binaries", getcwd(buf, 4096), "ASCII (*)").toStdString();
      relativePathPrefix = dialog.getExistingDirectory(0,  "get existing directory", getcwd(buf, 4096)).toStdString();
      i++;
    }

    tsv_directory = relativePathPrefix +"/"+tsv_directory;

  };
  
  if(is_directory( tsv_directory  ) == true )
  {
    RoseBin_Def::RoseAssemblyLanguage=RoseBin_Def::x86;
    RoseBin_Arch::arch=RoseBin_Arch::bit32;
    RoseBin_OS::os_sys=RoseBin_OS::linux_op;
    RoseBin_OS_VER::os_ver=RoseBin_OS_VER::linux_26;


    RoseFile* roseBin = new RoseFile( (char*)tsv_directory.c_str() );

    cerr << " ASSEMBLY LANGUAGE :: " << RoseBin_Def::RoseAssemblyLanguage << endl;
    // query the DB to retrieve all data

    globalBlock = roseBin->retrieve_DB();

    // traverse the AST and test it
    roseBin->test();
  }else{
    vector<char*> args;
    args.push_back(strdup(""));
    args.push_back(strdup(tsv_directory.c_str()));
    args.push_back(0);

    ostringstream outStr; 

    for(vector<char*>::iterator iItr = args.begin(); iItr != args.end();
        ++iItr )
    {
      outStr << *iItr << " ";
    }     
    ;
    std::cout << "Calling " << outStr.str() << std::endl;

    globalBlock =  frontend(args.size()-1,&args[0]);
    
  }
  return globalBlock;
};


std::pair<std::string,std::string> BinaryCloneGui::getAddressFromVectorsTable(uint64_t function_id, uint64_t index)
{
  std::string line;
  std::string offset;
  try {
    std::string selectQuery = "select line,offset from vectors where function_id=";
                selectQuery +=boost::lexical_cast<std::string>(function_id);
                selectQuery +=" and index_within_function=";
                selectQuery +=boost::lexical_cast<std::string>(index);
                selectQuery +=" limit 1";

    std::cout << "Query " << selectQuery << std::endl; 
    sqlite3_command cmd(con, selectQuery.c_str());
    sqlite3_reader datasets=cmd.executereader();
    datasets.read(); 
    line   = datasets.getstring(0);
    offset = datasets.getstring(1);

//    address = sqlite3x::sqlite3_command(con, selectQuery.c_str()).executestring();
  } catch (std::exception& ex) {std::cerr << "Exception Occurred: " << ex.what() << std::endl;}

  return std::pair<std::string,std::string>(line,offset);
}  ;



enum ExpressionCategory {ec_reg = 0, ec_mem = 1, ec_val = 2};

static const size_t numberOfInstructionKinds = x86_last_instruction;
inline size_t getInstructionKind(SgAsmx86Instruction* insn) {return insn->get_kind();}



inline ExpressionCategory getCategory(SgAsmExpression* e) {
  if (isSgAsmValueExpression(e)) {
    return ec_val;
  } else if (isSgAsmRegisterReferenceExpression(e)) {
    return ec_reg;
  } else if (isSgAsmMemoryReferenceExpression(e)) {
    return ec_mem;
  } else {
    abort();
  }
}

SgAsmExpressionPtrList& getOperands(SgAsmInstruction* insn) {
  SgAsmOperandList* ol = insn->get_operandList();
  SgAsmExpressionPtrList& operands = ol->get_operands();
  return operands;
}
static map<string, void*> internTable;

inline void* intern(const std::string& s) {
  map<string, void*>::const_iterator i = internTable.find(s);
  if (i == internTable.end()) {
    void* sCopy = new string(s);
    internTable.insert(std::make_pair(s, sCopy));
    return sCopy;
  } else {
    return i->second;
  }
}
static map<SgAsmExpression*, void*> unparseAndInternTable;

inline void* unparseAndIntern(SgAsmExpression* e) {
  map<SgAsmExpression*, void*>::const_iterator i = unparseAndInternTable.find(e);
  if (i == unparseAndInternTable.end()) {
    void* sPtr = intern(unparseExpression(e));
    unparseAndInternTable.insert(std::make_pair(e, sPtr));
    return sPtr;
  } else {
    return i->second;
  }
}
 
void numberOperands(std::vector<SgAsmx86Instruction*>::iterator beg,
        std::vector<SgAsmx86Instruction*>::iterator end, map<SgAsmExpression*, size_t> numbers[3]) {
  map<void*, size_t> stringNumbers[3];
  for (; beg != end; ++beg) {
    SgAsmx86Instruction* insn = *beg;
    const SgAsmExpressionPtrList& operands = getOperands(insn);
    //size_t operandCount = operands.size();
    for (size_t j = 0; j < operands.size(); ++j) {
      SgAsmExpression* e = operands[j];
      ExpressionCategory cat = getCategory(e);
      void* str = unparseAndIntern(e);
      map<void*, size_t>& currentStringNums = stringNumbers[(int)cat];
      map<void*, size_t>::const_iterator stringNumIter = currentStringNums.find(str);
      size_t num = (stringNumIter == currentStringNums.end() ? currentStringNums.size() : stringNumIter->second);
      if (stringNumIter == currentStringNums.end()) currentStringNums.insert(std::make_pair(str, num));
      numbers[(int)cat][e] = num;
    }
  }
}
std::string BinaryCloneGui::normalizeInstructionsToHTML(std::vector<SgAsmx86Instruction*>::iterator beg, 
    std::vector<SgAsmx86Instruction*>::iterator end)
{
    string normalizedUnparsedInstructions;
    map<SgAsmExpression*, size_t> valueNumbers[3];
    numberOperands( beg,end, valueNumbers);

    // Unparse the normalized forms of the instructions
    for (; beg != end; ++beg ) {
      SgAsmx86Instruction* insn = *beg;
      string mne = insn->get_mnemonic();
      boost::to_lower(mne);
      mne = "<font color=\"red\">" + htmlEscape(mne)+"</font>";

      normalizedUnparsedInstructions += mne;
      const SgAsmExpressionPtrList& operands = getOperands(insn);
      // Add to total for this variant
      // Add to total for each kind of operand
      size_t operandCount = operands.size();

      normalizedUnparsedInstructions += "<font color=\"blue\">";
      for (size_t i = 0; i < operandCount; ++i) {
        SgAsmExpression* operand = operands[i];
        ExpressionCategory cat = getCategory(operand);
        map<SgAsmExpression*, size_t>::const_iterator numIter = valueNumbers[(int)cat].find(operand);
        assert (numIter != valueNumbers[(int)cat].end());
        size_t num = numIter->second;

        normalizedUnparsedInstructions += (cat == ec_reg ? " R" : cat == ec_mem ? " M" : " V") + boost::lexical_cast<string>(num);
      }
      normalizedUnparsedInstructions += "; </font> <br> ";
  
    }
   
    return normalizedUnparsedInstructions;
};



string unparseX86InstructionToHTMLWithAddress(SgAsmx86Instruction* insn) {
  if (insn == NULL) return "BOGUS:NULL";
  string result = "<font color=\"green\">" + htmlEscape(StringUtility::intToHex(insn->get_address())) + "</font>:";
  result += "<font color=\"red\">" + htmlEscape(insn->get_mnemonic());
  switch (insn->get_branchPrediction()) {
    case x86_branch_prediction_none: break;
    case x86_branch_prediction_taken: result += ",pt"; break;
    case x86_branch_prediction_not_taken: result += ",pn"; break;
    default: ROSE_ASSERT (!"Bad branch prediction");
  }
  result += "</font>";
  result += std::string((result.size() >= 7 ? 1 : 7 - result.size()), ' ');
  SgAsmOperandList* opList = insn->get_operandList();
  const SgAsmExpressionPtrList& operands = opList->get_operands();
  for (size_t i = 0; i < operands.size(); ++i) {
    if (i != 0) result += ", ";
    result += "<font color=\"blue\">" + htmlEscape(unparseExpression(operands[i])) + "</font>";
  }
  return result;
}

void BinaryCloneGui::showClone(int row)
{

  //Disassemble files

  Element& elem = vectorOfClones[row];

 
  std::string beginAddressFileA = getAddressFromVectorsTable(elem.function_A, elem.begin_index_within_function_A).first;
  std::string beginAddressFileB = getAddressFromVectorsTable(elem.function_B, elem.begin_index_within_function_B).first;
  std::string endAddressFileA = getAddressFromVectorsTable(elem.function_A, elem.end_index_within_function_A).second;
  std::string endAddressFileB = getAddressFromVectorsTable(elem.function_B, elem.end_index_within_function_B).second;


  std::string addresses = beginAddressFileA + " " + endAddressFileA + " " + beginAddressFileB + " "
                          + endAddressFileB; 



  SgNode* fileA = disassembleFile(elem.file_A);
  SgNode* fileB = disassembleFile(elem.file_B);

  std::string unparsedFileA;
  std::string unparsedFileB;

  std::string normalizedFileA;
  std::string normalizedFileB;

  ROSE_ASSERT(fileA != NULL);
  ROSE_ASSERT(fileB != NULL);


  bool noFunctionsFileA = false;
  bool noFunctionsFileB = false;

  if( elem.file_A+"all-instructions" == elem.function_name_A)
    noFunctionsFileA = true;
   if( elem.file_B+"all-instructions" == elem.function_name_B)
    noFunctionsFileB = true;
 
  //std::cout << "Adddresses is : " << addresses << std::endl;
  bool first =true;
  while(true)
  {
    SgNode* currentRoot = first ? fileA : fileB;
   
    vector<SgAsmx86Instruction*> insns;
    FindInstructionsVisitor vis;

    if( (first & noFunctionsFileA ) | (!first & noFunctionsFileB)  )
      AstQueryNamespace::querySubTree(currentRoot, std::bind2nd( vis, &insns ));
    else
    {
      vector<SgAsmFunction*> funcs;
      FindAsmFunctionsVisitor funcVis;
      AstQueryNamespace::querySubTree(currentRoot, std::bind2nd( funcVis, &funcs ));

      for (size_t i = 0; i < funcs.size(); ++i) {
        if( funcs[i]->get_name() == (first ? elem.function_name_A : elem.function_name_B) )
        {
          AstQueryNamespace::querySubTree(funcs[i], std::bind2nd( vis, &insns ));
          break;
        }
      }

    }

    
    
    int beg = 0;
    int end = 0;


    std::string allInsnsUnparsed;
    
    for(size_t i=0; i < insns.size(); i++ )
    {
      allInsnsUnparsed += "<code>" + unparseX86InstructionToHTMLWithAddress(isSgAsmx86Instruction(insns[i])) + "</code>";
      allInsnsUnparsed += "<br> \n";

      //std::cout << insns[i]->get_address() << " " << beginAddressFileA << " " << (boost::lexical_cast<uint64_t>(beginAddressFileA)-insns[i]->get_address()) << std::endl; 
      if(insns[i]->get_address() == boost::lexical_cast<uint64_t>( first ? beginAddressFileA : beginAddressFileB ) ) 
        beg = i;
       if(insns[i]->get_address() == boost::lexical_cast<uint64_t>( first ? endAddressFileA : endAddressFileB ) ) 
       {
         end = i;
         break;
       }
    }

//    first ? normalizedFileA : normalizedFileB = normalizeInstructions(insns.begin()+beg, insns.begin()+end);

     std::cout <<  "INSNS : "  << insns.size()  << std::endl ;

     std::cout << "beg: "<< beg << " end: " << end << std::endl;
     
     std::string unparsed;

    for(int i =beg; i < end; i++)
    {
      //      unparsed += unparseInstructionWithAddress(insns[i]);
      
      unparsed += "<code>" + unparseX86InstructionToHTMLWithAddress(isSgAsmx86Instruction(insns[i])) + "</code>";
      unparsed += "<br> \n";
//      std::cout << "unparsed " << unparseInstructionWithAddress(insns[i]) << std::endl;
    }

    (first? codeWidget : codeWidget2 )->setPlainText(QString(unparsed.c_str()));

    std::string normalized = normalizeInstructionsToHTML(insns.begin()+beg, insns.begin()+end); 

    (first? unparsedView.first : unparsedView.second ) = unparsed;

    (first? normalizedView.first : normalizedView.second ) = normalized;
    (first? allInsnsUnparsedView.first : allInsnsUnparsedView.second ) = allInsnsUnparsed;

    selectView(0);
     
    //codeWidget->setReadOnly(true);
    //codeWidget->setPlainText(QString(normalizeInstructions(insns.begin()+beg, insns.begin()+end).c_str()));


   
    if(first == true) first = false;
    else break;
      
  }

  

  DeleteSgTree(fileA);
  DeleteSgTree(fileB);

}
  
void BinaryCloneGui::highlightRow(int row)
{
  activeRow = -1;

  if(row >= 0)
  {         
    QFont f = tableWidget->getFont(0, row);
    f.setBold(true);
    tableWidget->setFont(f, 0, row);
    activeRow = row;

    showClone(row);

    
    //tableWidget->isItemSelected(tableWidget->horizontalHeaderItem(row));

  } //if(row >= 0)

  return;
} //CompassGui::highlightRow(int row)

void BinaryCloneGui::unhighlightRow(int row)
{
  if (row >= 0) 
  {
    QFont f = tableWidget->getFont(0, row);
    f.setBold(false);
    tableWidget->setFont(f, 0, row);
  } //if (row >= 0)

  return;
} //CompassGui::unhighlightRow(int row)




BinaryCloneGui::~BinaryCloneGui()
{
  if( window ) delete window;

} //BinaryCloneGui::~BinaryCloneGui()



