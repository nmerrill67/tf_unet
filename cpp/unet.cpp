#include <unistd.h>
#include "unet.h"
#include "opencv2/imgproc.hpp"

void dealloc(void* data, size_t len, void* arg)
{
    free(data);
}

void free_buffer(void* data, size_t len)
{
    free(data);
}

TF_Buffer* read_tf_buffer(const char* file) {                                                  
    FILE *f = fopen(file, "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);                                                                  
    fseek(f, 0, SEEK_SET);  //same as rewind(f);                                            
    void* data = malloc(fsize);                                                             
    fread(data, fsize, 1, f);
    fclose(f);

    TF_Buffer* buf = TF_NewBuffer();                                                        
    buf->data = data;
    buf->length = fsize;                                                                    
    buf->data_deallocator = free_buffer;                                                    
    return buf;
} 

UNet::UNet() : w(160), h(120), c(3)
{
    printf("Found TensorFlow version %s\n", TF_Version());

    if (access("unet.pb", F_OK ) == -1)
    {
        fprintf(stderr, "unet.pb does not exist. Please freeze a graph for inference.\n");
    }
    
    graph = TF_NewGraph();
    status = TF_NewStatus();
    
    TF_Buffer* graph_def = read_tf_buffer("unet.pb");                      
    
    TF_SessionOptions* sopts = TF_NewSessionOptions();

    // Import graph_def into graph                                                          
    TF_ImportGraphDefOptions* opts = TF_NewImportGraphDefOptions();                         
    TF_GraphImportGraphDef(graph, graph_def, opts, status);
    
    const char* msg = TF_Message(status);
    int code = TF_GetCode(status);
    printf("\nLoaded graph. Status: %s\n\n", code ? msg : "SUCCESS");

    sess = TF_NewSession(graph, sopts, status);
    TF_DeleteSessionOptions(sopts);
    TF_DeleteImportGraphDefOptions(opts);
    TF_DeleteBuffer(graph_def);

    msg = TF_Message(status);
    code = TF_GetCode(status);
    printf("\nStarted session. Status: %s\n\n", code ? msg : "SUCCESS");
    
    in_op = {TF_GraphOperationByName(graph, 
            "UNet/images"), 0};
    if (!in_op.oper) {
        fprintf(stderr, "Can't init in_op.");
    } else {
        printf("Successfully initialized in_op\n");
    }

    out_op = {TF_GraphOperationByName(graph, 
           "mask"), 0};

    if (!out_op.oper) {
        fprintf(stderr, "Can't init out_op.");
    } else {
        printf("Successfully initialized out_op\n");
    }
}


UNet::~UNet()
{

    if (sess)
    {
        TF_CloseSession(sess, status);
        TF_DeleteSession(sess, status);
    }
    if (input)
        TF_DeleteTensor(input);
    if (output)
        TF_DeleteTensor(output);
    if (status)
        TF_DeleteStatus(status);
    printf("Successfully closed session and session data\n");

}

void UNet::run(const cv::Mat& _im, cv::Mat& out)
{

    cv::Size sz(w, h);
    cv::Mat im(sz, CV_32FC3);
    cv::resize(_im, im, sz);
    im /= 255.0;

    const int64_t dims[] = {1, h, w, c};

    input = TF_NewTensor(
        TF_FLOAT, dims,
        4, im.data, h*w*c*sizeof(float),
        &dealloc, NULL);

    if (!input)
    { 
        fprintf(stderr, "Failed to create input tensor\n");
        return;
    } else {
        printf("Successfully created input tensor\n");
    }


    TF_SessionRun(sess,
                  NULL, // Run options.
                  &in_op, &input, 1, // Input tensors, input tensor values, number of inputs.
                  &out_op, &output, 1, // Output tensors, output tensor values, number of outputs.
                  NULL, 0, // Target operations, number of targets.
                  NULL, // Run metadata.
                  status // Output status.
    );

    int code = TF_GetCode(status);
    if (code)
    {
        const char* msg = TF_Message(status);
        fprintf(stderr, "\nSession run error! Status: %s\n\n", msg);
        return;
    } else {
        printf("Successfully ran session\n");
    }
    
    float* ret_data = (float*)TF_TensorData(output); // No need to free. TF handles that
   
    uint8_t* ret_data_uint8 = (uint8_t*)malloc(w*h*sizeof(uint8_t));
    for (int i = 0; i < w*h; i++)
        ret_data_uint8[i] = (uint8_t)ret_data[i];

    out = cv::Mat(sz, CV_8UC1, ret_data_uint8);
}







