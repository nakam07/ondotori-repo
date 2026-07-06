//温度ロガー「T&D おんどとり RTR500Bシリーズ（親機：RTR500BW、子機：RTR-501BL）」を使用し温度管理を行う事を想定したコード(Gemini 出力)
//リアルタイム監視にはクラウドを利用し、保存用の「記録データ」のみはローカルPC上（Windows11のFTPサーバー機能）に送信されるように設定し運用する事を想定
//RTR500BWからFTP送信される「記録データ」(.trz) を監視し、コマンド（現時点では未確認）を使ってメーカー提供ソフト「T&D Graph」を操作し .csv に変換して保存フォルダへ転送するCのプログラム
#include <windows.h>    
#include <stdio.h>
#include <stdlib.h>
#include <shlwapi.h> 

// リンクエラーを防ぐためのライブラリ指定（Visual Studio用）
#pragma comment(lib, "shlwapi.lib") //Shlwapi.libはWindows 7以前の環境で広く使用.Windows 8以降ではPathCch系関数が推奨される

// --- 【設定項目】環境に合わせて変更してください --- //ファイル名に日本語を使用する場合、コード中の文字列関連の記述（関数等）はワイド文字版(Unicode可)に変更する
#define FTP_WATCH_DIR "C:\\inetpub\\ftproot\\rz_data"  // FTPでtrzが届くフォルダ
//#define SAVE_DIR     "C:\\Ondotori_CSV_Saved"           // CSVを保存したいフォルダ 
#define SAVE_DIR     "D:\\Ondotori_CSV_Saved"           // FTPサーバー用フォルダとCSV保存用フォルダのドライブを分ける
#define TD_GRAPH_PATH "C:\\Program Files (x86)\\T&D Graph\\TandDGraph.exe" // T&D Graphのパス

// 指定したファイルがTRZファイル（拡張子 .trz）かチェックする関数  //コード中に呼出無し(main関数内で拡張子付きで検索を実施している為不要)
//BOOL IsTrzFile(const char* filename) { //filenameは次行の関数で使用する際には(char*)で型変換を行う(constを外す)（コンパイル時のエラー対策）
    //const char* ext = strrchr(filename, '.');  //正常時の戻り値はfilenameの中で'.'が最後に出現した位置へのポインタ
    //if (ext != NULL) {  //NULLはエラー時の戻り値。
        //return (_stricmp(ext, ".trz") == 0); //2つの引数の値が一致する時の戻り値が0
   // }
   // return FALSE;
//}

// 実際にT&D Graphを呼び出してCSVに変換し、元のファイルを消去する関数
void ConvertTrzToCsv(const char* filename) {   //main関数内で呼出。filename = "*.trz" となる。「*」は9通りの中からユーザーが任意で設定出来る（例：”親機名_子機名_日時”）
    char trzPath[MAX_PATH];
    char csvPath[MAX_PATH];
    char cmdArgs[MAX_PATH * 3];

    // フルパスの組み立て
    sprintf_s(trzPath, sizeof(trzPath), "%s\\%s", FTP_WATCH_DIR, filename); //従って、trzPath ="FTP_WATCH_DIR\\*.trz" == searchPath となる
    // 拡張子を除いたファイル名を取得してCSVのパスを作成
    char nameWithoutExt[MAX_PATH];
    strcpy_s(nameWithoutExt, sizeof(nameWithoutExt), filename); 
    PathRemoveExtensionA(nameWithoutExt); //ファイル名から拡張子を削除する(nameWithoutExt == "*" となる)。GCC（MinGW）等を使用する場合は、コンパイル引数に -lshlwapi を明示的に指定してリンクする必要がある。更に、この関数は非推奨なので PathCchRemoveExtension（）の使用を検討する必要あり
    sprintf_s(csvPath, sizeof(csvPath), "%s\\%s.csv", SAVE_DIR, nameWithoutExt); //従って、csvPath = "SAVE_DIR\\*.csv" となる

    // FTPの書き込み完了を少し待つ（3秒待機）
    Sleep(3000);  //データロガー（親機）からFTP_WATCH_DIRへの書き込み(.trzファイル転送)という外部からの処理が完了するまで、このCプログラムの処理を一時停止する

    // コマンドライン引数の組み立て: /c=\"出力CSVパス" \"入力trzパス"  //現時点では未確認のコマンド
    sprintf_s(cmdArgs, sizeof(cmdArgs), " /c=\"%s\" \"%s\"", csvPath, trzPath); //従って、cmdArgs = "/c=\"SAVE_DIR\\*.csv\" \"FTP_WATCH_DIR\\*.trz\"" となる

    // T&D Graphをバックグラウンド（非表示）で起動する設定 //Windows標準のRPAツール「Power Automate for desktop」を使用
    STARTUPINFOA si; //作成するプロセスの情報を設定する構造体型変数の宣言
    PROCESS_INFORMATION pi; //作成するプロセスのハンドルやIDを格納する構造体型変数の宣言（値の設定ではなく、関数から取得する為に使用）
    ZeroMemory(&si, sizeof(si)); //内部でRtlZeroMemory()が呼び出され<minwinbase.h>、そこから更にmemset()が呼び出される<winnt.h>事により、処理が行われる
    si.dwFlags = STARTF_USESHOWWINDOW; //STARTUPINFOA 構造体 の wShowWindow メンバ を有効化する
    si.wShowWindow = SW_HIDE; // 画面を完全に非表示にする
    ZeroMemory(&pi, sizeof(pi));

    printf("[変換開始] %s を処理中...\n", filename);

    // プロセス（T&D Graph）の起動
    if (CreateProcessA(
            TD_GRAPH_PATH, //起動する実行ファイル名
            cmdArgs, //プロセスに渡すコマンドライン引数
            NULL, //SECURITY_ATTRIBUTE構造体へのポインタ。NULL設定の場合、プロセスは既定のセキュリティ記述子を取得し、プロセスハンドルは子プロセスには継承されない
            NULL, //SECURITY_ATTRIBUTE構造体へのポインタ。NULL設定の場合、プロセスのプライマリスレッドは既定のセキュリティ記述子を取得し、プライマリスレッドハンドルは子プロセスには継承されない
            FALSE, //呼出元プロセスの継承可能な各ハンドルは、新しいプロセスには継承されない
            0,     //プロセスの属性（スケジューリング優先度その他）を指定。０の場合、プロセスは呼出元のエラーモードと親のコンソールの両方を継承する。又、環境ブロックにはワイド文字は含まれない
            NULL,  //プロセスの環境ブロックを表す文字列へのポインタ。NULLの場合、呼出元プロセスの環境変数が使用される
            NULL, //プロセスのカレントディレクトリ（絶対パス）。NULLの場合、呼出元プロセスのカレントディレクトリを使用
            &si, //プロセスの標準入出力のハンドルが格納される
            &pi  //プロセス及びそのプライマリスレッドのハンドルとIDが格納される
    )) {  
        // 変換が終わるまで待つ（同期処理）
        WaitForSingleObject(pi.hProcess, INFINITE); //プロセス（T&D Graph）を同期オブジェクトと捉え、プロセスが完了しプロセスハンドルにシグナルが送られるまで待機（Cプログラムの処理を進めない）
        CloseHandle(pi.hProcess); //プロセス（T&D Graph）のハンドルを閉じる
        CloseHandle(pi.hThread); //プロセス（T&D Graph）のプライマリスレッドのハンドルを閉じる

        // CSVファイルが正しく生成されたか確認
        if (GetFileAttributesA(csvPath) != INVALID_FILE_ATTRIBUTES) {  // 正常時にはファイル属性定数が返される。INVALID_FILE_ATTRIBUTES は、エラー時の戻り値
            printf("[成功] CSVが生成されました: %s\n", csvPath);
            
            // 元のTRZファイルを削除 //削除せず、一定期間保存する運用方法を検討
            //if (DeleteFileA(trzPath)) {
               // printf("[削除] 元のTRZファイルを削除しました。\n");
           // }
        } else {
            printf("[エラー] CSVが生成されませんでした。パス等を確認してください。\n");
        }
    } else {
        printf("[エラー] T&D Graphの起動に失敗しました。エラーコード: %lu\n", GetLastError()); //エラーコードは10進数の符号無し整数(32bit)。この後にFormatMessage()を使用し可読性を高める事も検討
    }
}

int main() {
    // 保存先フォルダがなければ自動作成
    CreateDirectoryA( 
        SAVE_DIR, //作成するフォルダのパス。
        NULL //SECURITY_ATTRIBUTE構造体へのポインタ。NULL設定の場合、フォルダは既定のセキュリティ記述子を取得する
    );

    // フォルダの変更通知ハンドルを作成（ファイルの作成を監視）
    HANDLE hNotify = FindFirstChangeNotificationA( 
        FTP_WATCH_DIR, //監視するフォルダの絶対パス。
        FALSE, // 指定したフォルダのみを監視する。サブフォルダは監視しない
        FILE_NOTIFY_CHANGE_FILE_NAME // ファイル名の変更・追加・削除を監視
    );

    if (hNotify == INVALID_HANDLE_VALUE) { //FindFirstChangeNotificationA()のエラー時
        printf("フォルダの監視初期化に失敗しました。パスを確認してください。\n");
        return 1;
    }

    printf("FTPフォルダの監視を開始しました... (%s)\n", FTP_WATCH_DIR);
    printf("終了するには Ctrl + C を押してください。\n\n");

    // 前回処理したファイルを記憶して重複処理を防ぐための変数
    char lastProcessedFile[MAX_PATH] = "";

    // 常時監視ループ
    while (TRUE) {
        // フォルダに変化（ファイル追加など）が起きるまでプログラムを一時停止して待つ（CPU負荷0%）
        DWORD dwWaitStatus = WaitForSingleObject(hNotify, INFINITE);  //変更通知オブジェクトを同期オブジェクトと捉え、指定した状態が発生するまで待機
        if (dwWaitStatus == WAIT_OBJECT_0) { //指定したオブジェクトの状態が通知された時
            // 変化があったら、フォルダ内のファイルを探索
            WIN32_FIND_DATAA findData; //FindFirstFileA()、FindNextFileA()で検出されたデータを格納する構造体型変数の宣言
            char searchPath[MAX_PATH];
            sprintf_s(searchPath, sizeof(searchPath), "%s\\*.trz", FTP_WATCH_DIR); //従って、searchaPath==trzPathとなる
            HANDLE hFind = FindFirstFileA(searchPath, &findData); //指定ファイルパスと一致するファイルを検索。正常時には同一パターンのファイル名の検索用ハンドルが返され、findDataにファイル名が格納される
            if (hFind != INVALID_HANDLE_VALUE) { //INVALID_HANDLE_VALUEはエラー時の戻り値
                do {
                    // ドットディレクトリを除外
                    if (strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0) {  //strcmpの戻り値が0になるのは、2つの引数の値が一致する時
                        // 連続で同じファイルを検知するのを防ぐ簡易チェック
                        if (strcmp(lastProcessedFile, findData.cFileName) != 0) {
                            strcpy_s(lastProcessedFile, sizeof(lastProcessedFile), findData.cFileName); //次回反復処理の準備
                            
                            // 変換処理の実行
                            ConvertTrzToCsv(findData.cFileName);  //定義済みの処理関数を呼び出す。cFileName は フルパスではなくファイル名のみが入る為、"*.trz"となる
                        }
                    }
                } while (FindNextFileA(hFind, &findData)); //検索ハンドルhFindを利用してsearchPathと一致するファイルを再度検索。該当ファイルが存在した場合、findDataにファイル名を格納する
                FindClose(hFind); //検索ハンドルhFindを閉じる
            }

            // 次の通知を待つためにリセット
            FindNextChangeNotification(hNotify);
        }
    }
    
    FindCloseChangeNotification(hNotify); //変更通知ハンドルを閉じる
    return 0;
}
