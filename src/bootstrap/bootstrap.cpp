
#include "bootstrap.h"

namespace fs = boost::filesystem;


bool Bootstrap::rmDirectory(const std::string& directory_path) {

    try {
        // Check if the directory exists
        if (fs::exists(directory_path)) {
            // Remove the directory and its contents
            fs::remove_all(directory_path);
            LogPrint(BCLog::BOOTSTRAP,"-bootstrap: Directory removed successfully.\n");
        } else {
            LogPrint(BCLog::BOOTSTRAP,"-bootstrap: Directory does not exist.\n");
        }
    } catch (const fs::filesystem_error& ex) {
        LogPrintf("-bootstrap: Error removing directory: %s\n",ex.what());
        return false;
    }

    return true;
}

bool Bootstrap::isDirectory(const std::string& directory_path) {

    if (fs::exists(directory_path)) return true;

    return false;
}

// Callback function to write downloaded data to a file
size_t Bootstrap::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::ofstream* file = static_cast<std::ofstream*>(userp);
    file->write(static_cast<const char*>(contents), total_size);
    return total_size;
}

// Define a function to handle progress updates
int ProgressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    // Calculate progress percentage
    double progress = (dlnow > 0) ? ((double)dlnow / (double)dltotal) * 100.0 : 0.0;

    LogPrintf("-bootstrap: Download: %.2f%%\n", progress);
    uiInterface.ShowProgress(_("Verifying blocks..."), progress);

    return 0;
}

// Function to download a file using libcurl
bool Bootstrap::DownloadFile(const std::string& url, const std::string& outputFileName) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        LogPrint(BCLog::BOOTSTRAP,"-bootstrap: Error initializing libcurl.\n");
        return false;
    }

    std::ofstream outputFile(outputFileName, std::ios::binary);
    if (!outputFile.is_open()) {
        LogPrint(BCLog::BOOTSTRAP,"-bootstrap: Error opening output file.\n");
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L); // Verify peer's SSL certificate
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outputFile);
    // Set progress callback and enable progress meter
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, ProgressCallback);

    CURLcode res = curl_easy_perform(curl);

    curl_easy_cleanup(curl);
    outputFile.close();

    if (res != CURLE_OK) {
        LogPrintf("-bootstrap: Error downloading file: %s\n",curl_easy_strerror(res));
        return false;
    }

    return true;
}

bool Bootstrap::extractZip(const std::string& zipFilePath, const std::string& outputFolderPath) {

    //uiInterface.InitMessage(_("Extrating zip file"));

    // Open the zip file
    unzFile zipFile = unzOpen(zipFilePath.c_str());
    if (!zipFile) {
        LogPrintf("-bootstrap: Error opening zip file: %s\n",zipFilePath);
        return false;
    }

    // Create the output folder if it doesn't exist
    if (!ensureOutputFolder(outputFolderPath)) {
        LogPrintf("-bootstrap: Error creating output folder: %s\n",outputFolderPath);
        unzClose(zipFile);
        return false;
    }

    // Go through each file in the zip and extract it
    unz_global_info globalInfo;
    if (unzGetGlobalInfo(zipFile, &globalInfo) != UNZ_OK) {
        LogPrint(BCLog::BOOTSTRAP,"-bootstrap: Error getting global info from zip file.\n");
        unzClose(zipFile);
        return false;
    }

    for (uLong i = 0; i < globalInfo.number_entry; ++i) {
        char fileName[256];
        unz_file_info fileInfo;

        if (unzGetCurrentFileInfo(zipFile, &fileInfo, fileName, sizeof(fileName), nullptr, 0, nullptr, 0) != UNZ_OK) {
            LogPrint(BCLog::BOOTSTRAP,"-bootstrap: Error getting file info from zip file.\n");
            unzClose(zipFile);
            return false;
        }

        if (unzOpenCurrentFile(zipFile) != UNZ_OK) {
            LogPrint(BCLog::BOOTSTRAP,"-bootstrap: Error opening current file in zip.\n");
            unzClose(zipFile);
            return false;
        }

        std::string outputPath = std::string(outputFolderPath) + "/" + fileName;

        if(endsWithSlash(outputPath))
            ensureOutputFolder(outputPath);
        else{
            std::ofstream outFile(outputPath, std::ios::binary);
            if (!outFile.is_open()) {
                LogPrintf("-bootstrap: Error creating output file: %s\n",outputPath);
                unzCloseCurrentFile(zipFile);
                unzClose(zipFile);
                return false;
            }

            // Read and write the file data
            char buffer[4096];
            int bytesRead;

            while ((bytesRead = unzReadCurrentFile(zipFile, buffer, sizeof(buffer))) > 0) {
                outFile.write(buffer, bytesRead);
            }
            outFile.close();
        }


        unzCloseCurrentFile(zipFile);

        LogPrintf("-bootstrap: File extracted: %s\n",fileName);
        uiInterface.InitMessage("File extracted:" + std::string(fileName));

        if (unzGoToNextFile(zipFile) != UNZ_OK) {
            break;  // Reached the end of the zip file
        }
    }

    // Close the zip file
    unzClose(zipFile);
    LogPrint(BCLog::BOOTSTRAP,"-bootstrap: Zip extraction successful.\n");

    fs::remove(zipFilePath.c_str());
    return true;

}

bool Bootstrap::ensureOutputFolder(const std::string& outputPath) {
    try {
        if (!fs::exists(outputPath)) {
            // Create the directory if it doesn't exist
            fs::create_directories(outputPath);
        } else if (!fs::is_directory(outputPath)) {
            // If it exists but is not a directory, print an error
            LogPrintf("-bootstrap: Error: Output path '%s' is not a directory.\n",outputPath);
            return false;
        }
    } catch (const std::exception& e) {
        // Handle any exceptions that may occur during filesystem operations
        LogPrintf("-bootstrap: Error creating output folder: %s\n", e.what());
        return false;
    }

    return true;
}

bool Bootstrap::endsWithSlash(const std::string& str) {
    // Check if the string ends with '/'
    return !str.empty() && str.back() == '/';
}
