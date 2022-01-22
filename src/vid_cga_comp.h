int CGA_Composite_Table[1024];
int video_sharpness;
int video_ri, video_rq, video_gi, video_gq, video_bi, video_bq;

void update_cga16_color();
void Composite_Process(Bit32u blocks, Bit8u *rgbi, int render);
void Test_Composite_Process(Bit32u blocks, Bit8u *rgbi, int render);
