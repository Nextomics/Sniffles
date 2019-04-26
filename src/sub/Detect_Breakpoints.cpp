/*
 * Detect_Breapoints.cpp
 *
 *  Created on: Jun 19, 2015
 *      Author: fsedlaze
 */

#include "Detect_Breakpoints.h"
#include "../print/IPrinter.h"

void store_pos(vector<hist_str> &positions, long pos, std::string read_name) {
	for (size_t i = 0; i < positions.size(); i++) {
		if (abs(positions[i].position - pos) < Parameter::Instance()->min_length) {
			positions[i].hits++;
			positions[i].names.push_back(read_name);
			return;
		}
	}
	hist_str tmp;
	tmp.position = pos;
	tmp.hits = 1;
	tmp.names.push_back(read_name);
	positions.push_back(tmp);
}

std::string reverse_complement(std::string sequence) {
	std::string tmp_seq;
	for (std::string::reverse_iterator i = sequence.rbegin(); i != sequence.rend(); i++) {
		switch ((*i)) {
		case 'A':
			tmp_seq += 'T';
			break;
		case 'C':
			tmp_seq += 'G';
			break;
		case 'G':
			tmp_seq += 'C';
			break;
		case 'T':
			tmp_seq += 'A';
			break;
		default:
			tmp_seq += (*i);
			break;
		}
	}
	return tmp_seq;
}

Breakpoint * split_points(vector<std::string> names, std::map<std::string, read_str> support) {
	std::map<std::string, read_str> new_support;
	for (size_t i = 0; i < names.size(); i++) {
		new_support[names[i]] = support[names[i]];
	}
	position_str svs;
	svs.start.min_pos = 0; //just to initialize. Should not be needed anymore at this stage of the prog.
	svs.stop.max_pos = 0;
	svs.support = new_support;
	Breakpoint * point = new Breakpoint(svs, (*new_support.begin()).second.coordinates.second - (*new_support.begin()).second.coordinates.first);
	return point;
}

void detect_merged_svs(position_str point, RefVector ref, vector<Breakpoint *> & new_points) {
	new_points.clear(); //just in case!
	vector<hist_str> pos_start;
	vector<hist_str> pos_stop;
	for (std::map<std::string, read_str>::iterator i = point.support.begin(); i != point.support.end(); ++i) {
		store_pos(pos_start, (*i).second.coordinates.first, (*i).first);
		store_pos(pos_stop, (*i).second.coordinates.second, (*i).first);
	}

	int start_count = 0;
	for (size_t i = 0; i < pos_start.size(); i++) {
		//std::cout<<pos_start[i].hits <<",";
		if (pos_start[i].hits > Parameter::Instance()->min_support) {
			start_count++;

		}
	}
	int stop_count = 0;
	for (size_t i = 0; i < pos_stop.size(); i++) {
		//	std::cout << pos_stop[i].hits << ",";
		if (pos_stop[i].hits > Parameter::Instance()->min_support) {
			stop_count++;
		}
	}
	if (stop_count > 1 || start_count > 1) {
		std::cout << "\tprocessing merged TRA" << std::endl;
		if (start_count > 1) {
			new_points.push_back(split_points(pos_start[0].names, point.support));
			new_points.push_back(split_points(pos_start[1].names, point.support));
		} else {
			new_points.push_back(split_points(pos_stop[0].names, point.support));
			new_points.push_back(split_points(pos_stop[1].names, point.support));
		}
	}
}

std::string TRANS_type(char type) {
	string tmp;
	if (type & DEL) {
		tmp += "DEL";
	}
	if (type & INV) {
		if (!tmp.empty()) {
			tmp += '/';
		}
		tmp += "INV";
	}
	if (type & DUP) {
		if (!tmp.empty()) {
			tmp += '/';
		}
		tmp += "DUP";
	}
	if (type & INS) {
		if (!tmp.empty()) {
			tmp += '/';
		}
		tmp += "INS";
	}
	if (type & TRA) {
		if (!tmp.empty()) {
			tmp += '/';
		}
		tmp += "TRA";
	}
	if (type & NEST) {
		if (!tmp.empty()) {
			tmp += '/';
		}
		tmp += "NEST";
	}
	return tmp; // should not occur!
}

long get_ref_lengths(int id, RefVector ref) {
	long length = 0;

	for (size_t i = 0; i < (size_t) id && i < ref.size(); i++) {
		length += (long) ref[i].RefLength + (long) Parameter::Instance()->max_dist;
	}
	return length;
}

bool should_be_stored(Breakpoint *& point) {
	point->calc_support(); // we need that before:
	//std::cout << "Stored: " << point->get_support() << " " << point->get_length() << std::endl;
	if (point->get_SVtype() & TRA) { // we cannot make assumptions abut support yet.
		point->set_valid((bool) (point->get_support() > 1)); // this is needed as we take each chr independently and just look at the primary alignment
	} else if (point->get_support() >= Parameter::Instance()->min_support) {
		point->predict_SV();
		point->set_valid((bool) (point->get_length() > Parameter::Instance()->min_length));
	}
	return point->get_valid();
}
void polish_points(std::vector<Breakpoint *> & points, RefVector ref) { //TODO might be usefull! but why does the tree not fully work??
	return;
	for (size_t i = 0; i < points.size(); i++) {
		if (points[i]->get_SVtype() & INS && (points[i]->get_length() == Parameter::Instance()->huge_ins)) {
			for (size_t j = 0; j < points.size(); j++) {
				if (i != j) {
					if (abs(points[i]->get_coordinates().start.min_pos - points[j]->get_coordinates().start.min_pos) < Parameter::Instance()->max_dist || abs(points[i]->get_coordinates().stop.max_pos - points[j]->get_coordinates().stop.max_pos) < Parameter::Instance()->max_dist) {
						std::cout << "HIT!: " << points[j]->get_coordinates().start.min_pos << " " << points[i]->get_coordinates().start.min_pos << " " << points[j]->get_coordinates().stop.max_pos << " " << points[i]->get_coordinates().stop.max_pos << " len: " << points[j]->get_length() << " " << points[i]->get_length() << std::endl;
						break;
					}
				}

			}
		}
	}
}

void write_read(Alignment * tmp_aln, FILE * & ref_allel_reads) {
/*	tmp.chr_id = tmp_aln->getRefID();	//check string in binary???
	tmp.start = tmp_aln->getPosition();
	tmp.length = tmp_aln->getRefLength();
	if (tmp_aln->getStrand()) {
		tmp.strand = 1;
	} else {
		tmp.strand = 2;
	}*/

	fprintf(ref_allel_reads, "%i",tmp_aln->getRefID());
	fprintf(ref_allel_reads, "%c",'\t');
	fprintf(ref_allel_reads, "%i",tmp_aln->getPosition());
	fprintf(ref_allel_reads, "%c",'\t');
	fprintf(ref_allel_reads, "%i",tmp_aln->getRefLength());
	fprintf(ref_allel_reads, "%c",'\t');
	if (tmp_aln->getStrand()) {
		fprintf(ref_allel_reads, "%c",'1');
	} else {
		fprintf(ref_allel_reads, "%c",'2');
	}
	fprintf(ref_allel_reads, "%c",'\n');
}

/*GrandOmics comment
	detect sv breakpoint and insert into interval tree
	1. estimate parameters using randomly chosen 1000 aligned reads
	2. parse bam file, process 10000 reads as batch and switch 
	   chromosome during processing
	   - analyze aln event and split event
	   - add aln and split event into iterval tree
	3. get breakpoints and calculate sv support
*/
void detect_breakpoints(std::string read_filename, IPrinter *& printer) {
	estimate_parameters(read_filename);
	BamParser * mapped_file = 0;
	RefVector ref;
	if (read_filename.find("bam") != string::npos) {
		mapped_file = new BamParser(read_filename);
		ref = mapped_file->get_refInfo();
	} else {
		cerr << "File Format not recognized. File must be a sorted .bam file!" << endl;
		exit(EXIT_FAILURE);
	}
//Using PlaneSweep to comp coverage and iterate through reads:
//PlaneSweep * sweep = new PlaneSweep();
	//Using Interval tree to store and manage breakpoints:

	IntervallTree final;
	IntervallTree bst;

	TNode * root_final = NULL;
	int current_RefID = 0;


	TNode *root = NULL;
//FILE * alt_allel_reads;
	FILE * ref_allel_reads;
	if (Parameter::Instance()->genotype) {
		ref_allel_reads = fopen(Parameter::Instance()->tmp_genotyp.c_str(), "w");
		//	ref_allel_reads = fopen(Parameter::Instance()->tmp_genotyp.c_str(), "wb");
	}
	Alignment * tmp_aln = mapped_file->parseRead(Parameter::Instance()->min_mq);
	long ref_space = get_ref_lengths(tmp_aln->getRefID(), ref);
	long num_reads = 0;

	/*Genotyper * go;
	 if (Parameter::Instance()->genotype) {
	 go = new Genotyper();
	 }*/
	std::cout << "Start parsing... " << ref[tmp_aln->getRefID()].RefName << std::endl;

	while (!tmp_aln->getQueryBases().empty()) {

		if ((tmp_aln->getAlignment()->IsPrimaryAlignment()) && (!(tmp_aln->getAlignment()->AlignmentFlag & 0x800) && tmp_aln->get_is_save())) {	// && (Parameter::Instance()->chr_names.empty() || Parameter::Instance()->chr_names.find(ref[tmp_aln->getRefID()].RefName) != Parameter::Instance()->chr_names.end())) {

			//change CHR:
			if (current_RefID != tmp_aln->getRefID()) {

				std::cout << "\tSwitch Chr " << ref[tmp_aln->getRefID()].RefName << std::endl;	//" " << ref[tmp_aln->getRefID()].RefLength
				std::vector<Breakpoint *> points;
				bst.get_breakpoints(root, points);
				//polish_points(points, ref);

				/*	if (Parameter::Instance()->genotype) {
				 fclose(ref_allel_reads);
				 cout<<"\t\tGenotyping"<<endl;
				 go->update_SVs(points, ref_space);
				 cout<<"\t\tGenotyping finished"<<endl;
				 ref_allel_reads = fopen(Parameter::Instance()->tmp_genotyp.c_str(), "wb");
				 }*/

				for (int i = 0; i < points.size(); i++) {
					points[i]->calc_support();
					if (points[i]->get_valid()) {
						//invoke update over ref support!
						if (points[i]->get_SVtype() & TRA) {
							final.insert(points[i], root_final);
						} else {
							printer->printSV(points[i]);
						}
					}
				}
				bst.clear(root);
				current_RefID = tmp_aln->getRefID();
				ref_space = get_ref_lengths(tmp_aln->getRefID(), ref);
			}

			//SCAN read:
			std::vector<str_event> aln_event;
			std::vector<aln_str> split_events;
			if (tmp_aln->getMappingQual() > Parameter::Instance()->min_mq) {
				double score = tmp_aln->get_scrore_ratio();

				//

#pragma omp parallel // starts a new team
				{
#pragma omp sections
					{
#pragma omp section
						//GrandOmics comment: refer to Alignment.cpp
						//summarize sv events, see struct str_event for detail
						{
							//		clock_t begin = clock();
							if ((score == -1 || score > Parameter::Instance()->score_treshold)) {
								aln_event = tmp_aln->get_events_Aln();
							}
							//		Parameter::Instance()->meassure_time(begin, " Alignment ");
						}
#pragma omp section
						{
							//	clock_t begin_split = clock();
							split_events = tmp_aln->getSA(ref);
							//	Parameter::Instance()->meassure_time(begin_split, " Split reads ");
						}
					}
				}
				//tmp_aln->set_supports_SV(aln_event.empty() && split_events.empty());

				//Store reference supporting reads for genotype estimation:

				bool SV_support = (!aln_event.empty() && !split_events.empty());
				if (Parameter::Instance()->genotype && !SV_support) {
				//	cout << "STORE" << endl;
					//write read:
					/*str_read tmp;
					tmp.chr_id = tmp_aln->getRefID();	//check string in binary???
					tmp.start = tmp_aln->getPosition();
					tmp.length = tmp_aln->getRefLength();
					if (tmp_aln->getStrand()) {
						tmp.strand = 1;
					} else {
						tmp.strand = 2;
					}*/
					write_read(tmp_aln, ref_allel_reads);
					//fwrite(&tmp, sizeof(struct str_read), 1, ref_allel_reads);
				}

				//GrandOmics comment
				//store the potential SVs:
				if (!aln_event.empty()) {
					add_events(tmp_aln, aln_event, 0, ref_space, bst, root, num_reads, false);
				}
				if (!split_events.empty()) {
					add_splits(tmp_aln, split_events, 1, ref, bst, root, num_reads, false);
				}
			}
		}
		//get next read:
		mapped_file->parseReadFast(Parameter::Instance()->min_mq, tmp_aln);

		num_reads++;

		if (num_reads % 10000 == 0) {
			cout << "\t\t# Processed reads: " << num_reads << endl;
		}
	}
//filter and copy results:
	std::cout << "Finalizing  .." << std::endl;
	std::vector<Breakpoint *> points;
	bst.get_breakpoints(root, points);

	/*	if (Parameter::Instance()->genotype) {
	 fclose(ref_allel_reads);
	 go->update_SVs(points, ref_space);
	 string del = "rm ";
	 del += Parameter::Instance()->tmp_genotyp;
	 del += "ref_allele";
	 system(del.c_str());
	 }*/
	
	//GrandOmics comment
	//check breakpoints support and add valid point into final iterval tree
	//refer to Breakpoint.cpp
	for (int i = 0; i < points.size(); i++) {
		points[i]->calc_support();
		if (points[i]->get_valid()) {
			//invoke update over ref support!
			if (points[i]->get_SVtype() & TRA) {
				final.insert(points[i], root_final);
			} else {
				printer->printSV(points[i]);
			}
		}
	}
	bst.clear(root);
	points.clear();
	final.get_breakpoints(root_final, points);
	//std::cout<<"Detect merged tra"<<std::endl;
	size_t points_size = points.size();
	for (size_t i = 0; i < points_size; i++) { // its not nice, but I may alter the length of the vector within the loop.
		if (points[i]->get_SVtype() & TRA) {
			vector<Breakpoint *> new_points;
			//GrandOmics comment
			//Detect falsely merged svs for TRA event
			detect_merged_svs(points[i]->get_coordinates(), ref, new_points);
			if (!new_points.empty()) {							// I only allow for 1 split!!
				points[i] = new_points[0];
				points.push_back(new_points[1]);
			}
		}
	}
	//std::cout<<"fin up"<<std::endl;
	for (size_t i = 0; i < points.size(); i++) {
		if (points[i]->get_SVtype() & TRA) {
			points[i]->calc_support();
			points[i]->predict_SV();
		}
		if (points[i]->get_support() >= Parameter::Instance()->min_support && points[i]->get_length() > Parameter::Instance()->min_length) {
			printer->printSV(points[i]);
		}
	}
	//std::cout<<"Done"<<std::endl;
	if (Parameter::Instance()->genotype) {
		fclose(ref_allel_reads);
	}
}

/*GrandOmics comment
	add breakpoints into interval tree
*/
void add_events(Alignment *& tmp, std::vector<str_event> events, short type, long ref_space, IntervallTree & bst, TNode *&root, long read_id, bool add) {

	bool flag = (strcmp(tmp->getName().c_str(), Parameter::Instance()->read_name.c_str()) == 0);
	for (size_t i = 0; i < events.size(); i++) {
		//	if (events[i - 1].length > Parameter::Instance()->min_segment_size || events[i].length > Parameter::Instance()->min_segment_size) {
		position_str svs;
		read_str read;
		if (events[i].is_noise) {
			read.type = 2;
		} else {
			read.type = 0;
		}
		read.SV = events[i].type;
		read.sequence = events[i].sequence;

		if (flag) {
			std::cout << "ADD EVENT " << tmp->getName() << " " << tmp->getRefID() << " " << events[i].pos << " " << abs(events[i].length) << std::endl;
		}
		svs.start.min_pos = (long) events[i].pos + ref_space;
		svs.stop.max_pos = svs.start.min_pos + events[i].length;

		if (tmp->getStrand()) {
			read.strand.first = (tmp->getStrand());
			read.strand.second = !(tmp->getStrand());
		} else {
			read.strand.first = !(tmp->getStrand());
			read.strand.second = (tmp->getStrand());
		}
		//	start.support[0].read_start.min = events[i].read_pos;

		read.read_strand.first = tmp->getStrand();
		read.read_strand.second = tmp->getStrand();
		if (flag) {
			std::cout << tmp->getName() << " " << tmp->getRefID() << " " << svs.start.min_pos << " " << svs.stop.max_pos << " " << svs.stop.max_pos - svs.start.min_pos << std::endl;
		}

		if (svs.start.min_pos > svs.stop.max_pos) {
			//can this actually happen?
			read.coordinates.first = svs.stop.max_pos;
			read.coordinates.second = svs.start.min_pos;
		} else {
			read.coordinates.first = svs.start.min_pos;
			read.coordinates.second = svs.stop.max_pos;
		}

		svs.start.max_pos = svs.start.min_pos;
		svs.stop.min_pos = svs.stop.max_pos;

		if (svs.start.min_pos > svs.stop.max_pos) { //incase they are inverted
			svs_breakpoint_str pos = svs.start;
			svs.start = svs.stop;
			svs.stop = pos;
			pair<bool, bool> tmp = read.strand;
			read.strand.first = tmp.second;
			read.strand.second = tmp.first;
		}

		//TODO: we might not need this:
		if (svs.start.min_pos > svs.stop.max_pos) {
			read.coordinates.first = svs.stop.max_pos;
			read.coordinates.second = svs.start.min_pos;
		} else {
			read.coordinates.first = svs.start.min_pos;
			read.coordinates.second = svs.stop.max_pos;
		}

		read.id = read_id;
		svs.support[tmp->getName()] = read;
		svs.support[tmp->getName()].length = events[i].length;
		Breakpoint * point = new Breakpoint(svs, events[i].length);
		if (add) {
			bst.insert_existant(point, root);
		} else {
			bst.insert(point, root);
		}
		//std::cout<<"Print:"<<std::endl;
		//bst.print(root);
	}
//	}
}

/*GrandOmics comment
	analyze split alignment event, and add points into interval tree
*/
void add_splits(Alignment *& tmp, std::vector<aln_str> events, short type, RefVector ref, IntervallTree& bst, TNode *&root, long read_id, bool add) {
	bool flag = (strcmp(tmp->getName().c_str(), Parameter::Instance()->read_name.c_str()) == 0);

	if (flag) {
		cout << "SPLIT: " << std::endl;
		for (size_t i = 0; i < events.size(); i++) {
			std::cout << events[i].pos << " stop: " << events[i].pos + events[i].length << " " << events[i].RefID << " READ: " << events[i].read_pos_start << " " << events[i].read_pos_stop;
			if (events[i].strand) {
				cout << " +" << endl;
			} else {
				cout << " -" << endl;
			}
		}
	}

	for (size_t i = 1; i < events.size(); i++) {
		//	if (events[i - 1].length > Parameter::Instance()->min_segment_size || events[i].length > Parameter::Instance()->min_segment_size) {
		position_str svs;
		//position_str stop;
		read_str read;
		read.sequence = "NA";
		//read.name = tmp->getName();
		read.type = type;
		read.SV = 0;
		read.read_strand.first = events[i - 1].strand;
		read.read_strand.second = events[i].strand;

		//stop.support.push_back(read);

		//GrandOmics comment
		//split alignment events lay on same reference read => insertion/deletion/duplication/inversion
		if (events[i].RefID == events[i - 1].RefID) { //IF different chr -> tra
			if (events[i - 1].strand == events[i].strand) { //IF same strand -> del/ins/dup
				if (events[i - 1].strand) {
					read.strand.first = events[i - 1].strand;
					read.strand.second = !events[i].strand;
				} else {
					read.strand.first = !events[i - 1].strand;
					read.strand.second = events[i].strand;
				}
				//	int len1 = 0;
				//int len2 = 0;
				svs.read_start = events[i - 1].read_pos_stop; // (short) events[i - 1].read_pos_start + (short) events[i - 1].length;
				svs.read_stop = events[i].read_pos_start;
				if (events[i - 1].strand) {
					svs.start.min_pos = events[i - 1].pos + events[i - 1].length + get_ref_lengths(events[i - 1].RefID, ref);
					svs.stop.max_pos = events[i].pos + get_ref_lengths(events[i].RefID, ref);
				} else {
					svs.start.min_pos = events[i].pos + events[i].length + get_ref_lengths(events[i].RefID, ref);
					svs.stop.max_pos = events[i - 1].pos + get_ref_lengths(events[i - 1].RefID, ref);
				}

				if (flag) {
					cout << "Debug: SV_Size: " << (svs.start.min_pos - svs.stop.max_pos) << " tmp: " << (svs.stop.max_pos - svs.start.min_pos) << " Ref_start: " << svs.start.min_pos - get_ref_lengths(events[i].RefID, ref) << " Ref_stop: " << svs.stop.max_pos - get_ref_lengths(events[i].RefID, ref) << " readstart: " << svs.read_start << " readstop: " << svs.read_stop << std::endl;
				}
				//GrandOmics comment
				//Insertion type rules
				if ((svs.stop.max_pos - svs.start.min_pos) > Parameter::Instance()->min_length * -1 && ((svs.stop.max_pos - svs.start.min_pos) + (Parameter::Instance()->min_length) < (svs.read_stop - svs.read_start) && (svs.read_stop - svs.read_start) > (Parameter::Instance()->min_length * 2))) {
					if (!events[i].cross_N || (double) ((svs.stop.max_pos - svs.start.min_pos) + Parameter::Instance()->min_length) < ((double) (svs.read_stop - svs.read_start) * Parameter::Instance()->avg_ins)) {
						svs.stop.max_pos += (svs.read_stop - svs.read_start); //TODO check!
						if (Parameter::Instance()->print_seq) {
							svs.read_stop = events[i].read_pos_start;
							svs.read_start = events[i - 1].read_pos_stop;
							if (svs.read_stop > tmp->getAlignment()->QueryBases.size()) {
								cerr << "BUG: split read ins! " << svs.read_stop << " " << tmp->getAlignment()->QueryBases.size() << " " << tmp->getName() << endl;
							}
							if (!events[i - 1].strand) {
								std::string tmp_seq = reverse_complement(tmp->getAlignment()->QueryBases);

								read.sequence = reverse_complement(tmp_seq.substr(svs.read_start, svs.read_stop - svs.read_start));
							} else {
								read.sequence = tmp->getAlignment()->QueryBases.substr(svs.read_start, svs.read_stop - svs.read_start);
							}
							if (flag) {
								cout << "INS: " << endl;
								cout << "split read ins! " << events[i - 1].read_pos_stop << " " << events[i].read_pos_start << " " << " " << tmp->getAlignment()->QueryBases.size() << " " << tmp->getName() << endl;
								cout << "Seq+:" << read.sequence << endl;
							}
						}
						read.SV |= INS;
					} else {
						read.SV |= 'n';
					}
				//GrandOmics comment
				//Deletion type rules
				} else if ((svs.start.min_pos - svs.stop.max_pos) * -1 > (svs.read_stop - svs.read_start) + (Parameter::Instance()->min_length)) {
					if (!events[i].cross_N || (double) (svs.start.min_pos - svs.stop.max_pos) * Parameter::Instance()->avg_del * -1.0 > (double) ((svs.read_stop - svs.read_start) + (Parameter::Instance()->min_length))) {
						read.SV |= DEL;
						if (flag) {
							cout << "DEL2" << endl;
						}
					} else {
						read.SV |= 'n';
					}
				//GrandOmics comment
				//Duplication type rules
				} else if ((svs.start.min_pos - svs.stop.max_pos) > Parameter::Instance()->min_length && (svs.read_start - svs.read_stop) < Parameter::Instance()->min_length) { //check with respect to the coords of reads!
					if (flag) {
						cout << "DUP: " << endl;
					}
					read.SV |= DUP;
				} else {
					if (flag) {
						cout << "N" << endl;
					}
					read.SV = 'n';
				}
			} else { // if first part of read is in a different direction as the second part-> INV

				read.strand.first = events[i - 1].strand;
				read.strand.second = !events[i].strand;

				bool is_overlapping = overlaps(events[i - 1], events[i]);
				if (is_overlapping && (events[i - 1].length > Parameter::Instance()->min_segment_size || events[i].length > Parameter::Instance()->min_segment_size)) {
					if (flag) {
						std::cout << "Overlap curr: " << events[i].pos << " " << events[i].pos + events[i].length << " prev: " << events[i - 1].pos << " " << events[i - 1].pos + events[i - 1].length << " " << tmp->getName() << std::endl;
					}
					read.SV |= NEST;

					if (events[i - 1].strand) {
						svs.start.min_pos = events[i - 1].pos + events[i - 1].length + get_ref_lengths(events[i - 1].RefID, ref);
						svs.stop.max_pos = (events[i].pos + events[i].length) + get_ref_lengths(events[i].RefID, ref);
					} else {
						svs.start.min_pos = events[i - 1].pos + get_ref_lengths(events[i - 1].RefID, ref);
						svs.stop.max_pos = events[i].pos + get_ref_lengths(events[i].RefID, ref);
					}

					if (svs.start.min_pos > svs.stop.max_pos) {
						long tmp = svs.start.min_pos;
						svs.start.min_pos = svs.stop.max_pos;
						svs.stop.max_pos = tmp;
					}
				} else if (!is_overlapping) {
					read.SV |= INV;
					if (events[i - 1].strand) {
						svs.start.min_pos = events[i - 1].pos + events[i - 1].length + get_ref_lengths(events[i - 1].RefID, ref);
						svs.stop.max_pos = (events[i].pos + events[i].length) + get_ref_lengths(events[i].RefID, ref);
					} else {
						svs.start.min_pos = events[i - 1].pos + get_ref_lengths(events[i - 1].RefID, ref);
						svs.stop.max_pos = events[i].pos + get_ref_lengths(events[i].RefID, ref);
					}
				}
			}
		//GrandOmics comment
		//split alignment events lay on different reference read => translocation
		} else { //if not on the same chr-> TRA
			read.strand.first = events[i - 1].strand;
			read.strand.second = !events[i].strand;
			if (events[i - 1].strand == events[i].strand) {
				//check this with + - strands!!

				if (events[i - 1].strand) {
					svs.start.min_pos = events[i - 1].pos + events[i - 1].length + get_ref_lengths(events[i - 1].RefID, ref);
					svs.stop.max_pos = events[i].pos + get_ref_lengths(events[i].RefID, ref);
				} else {
					svs.start.min_pos = events[i - 1].pos + get_ref_lengths(events[i - 1].RefID, ref);
					svs.stop.max_pos = events[i].pos + events[i].length + get_ref_lengths(events[i].RefID, ref);
				}
			} else {
				if (events[i - 1].strand) {
					svs.start.min_pos = events[i - 1].pos + events[i - 1].length + get_ref_lengths(events[i - 1].RefID, ref);
					svs.stop.max_pos = events[i].pos + events[i].length + get_ref_lengths(events[i].RefID, ref);
				} else {
					svs.start.min_pos = events[i - 1].pos + get_ref_lengths(events[i - 1].RefID, ref);
					svs.stop.max_pos = events[i].pos + get_ref_lengths(events[i].RefID, ref);
				}
			}
			read.SV |= TRA;
		}

		if (read.SV != 'n') {
			if (flag) {
				std::cout << "SPLIT: " << TRANS_type(read.SV) << " start: " << svs.start.min_pos - get_ref_lengths(events[i].RefID, ref) << " stop: " << svs.stop.max_pos - get_ref_lengths(events[i].RefID, ref);
				if (events[i - 1].strand) {
					std::cout << " +";
				} else {
					std::cout << " -";
				}
				if (events[i].strand) {
					std::cout << " +";
				} else {
					std::cout << " -";
				}
				std::cout << " " << tmp->getName() << std::endl;
				std::cout << "READ: " << svs.read_start << " " << svs.read_stop << " " << svs.read_start - svs.read_stop << std::endl;
			}
			//std::cout<<"split"<<std::endl;

			svs.start.max_pos = svs.start.min_pos;
			svs.stop.min_pos = svs.stop.max_pos;
			if (svs.start.min_pos > svs.stop.max_pos) {
				//maybe we have to invert the directions???
				svs_breakpoint_str pos = svs.start;
				svs.start = svs.stop;
				svs.stop = pos;

				pair<bool, bool> tmp = read.strand;

				read.strand.first = tmp.second;
				read.strand.second = tmp.first;
			}

			//TODO: we might not need this:
			if (svs.start.min_pos > svs.stop.max_pos) {
				read.coordinates.first = svs.stop.max_pos;
				read.coordinates.second = svs.start.min_pos;
			} else {
				read.coordinates.first = svs.start.min_pos;
				read.coordinates.second = svs.stop.max_pos;
			}

			//pool out?
			read.id = read_id;
			svs.support[tmp->getName()] = read;
			svs.support[tmp->getName()].length = abs(read.coordinates.second - read.coordinates.first);
			Breakpoint * point = new Breakpoint(svs, abs(read.coordinates.second - read.coordinates.first));
			//std::cout<<"split ADD: " << <<" Name: "<<tmp->getName()<<" "<< svs.start.min_pos- get_ref_lengths(events[i].RefID, ref)<<"-"<<svs.stop.max_pos- get_ref_lengths(events[i].RefID, ref)<<std::endl;
			if (add) {
				bst.insert_existant(point, root);
			} else {
				bst.insert(point, root);
			}
			//	std::cout<<"Print:"<<std::endl;
			//	bst.print(root);
		}
	}
	//}
}

/*GrandOmics comment
	randomly choose 1000 reads and estimate distribution of different pattern parameters
*/
void estimate_parameters(std::string read_filename) {
	if (Parameter::Instance()->skip_parameter_estimation) {
		return;
	}
	cout << "Estimating parameter..." << endl;
	//GrandOmics comment: we can use htslib library to parse bam file
	BamParser * mapped_file = 0;
	RefVector ref;
	if (read_filename.find("bam") != string::npos) {
		mapped_file = new BamParser(read_filename);
		ref = mapped_file->get_refInfo();
	} else {
		cerr << "File Format not recognized. File must be a sorted .bam file!" << endl;
		exit(EXIT_FAILURE);
	}

	Alignment * tmp_aln = mapped_file->parseRead(Parameter::Instance()->min_mq);
	double num = 0;
	double avg_score = 0;
	double avg_mis = 0;
	double avg_indel = 0;
	double avg_diffs_perwindow = 0;
	vector<int> mis_per_window; //histogram over #differences
	vector<int> scores;
//	std::string curr, prev = "";
	double avg_dist = 0;
	double tot_avg_ins = 0;
	double tot_avg_del = 0;
	while (!tmp_aln->getQueryBases().empty() && num < 1000) {	//1000
		//	std::cout<<"test "<<tmp_aln->getName()<<std::endl;
		if (rand() % 100 < 20 && ((tmp_aln->getAlignment()->IsPrimaryAlignment()) && (!(tmp_aln->getAlignment()->AlignmentFlag & 0x800)))) {				//}&& tmp_aln->get_is_save()))) {
			//1. check differences in window => min_treshold for scanning!
			//2. get score ration without checking before hand! (above if!)
			double dist = 0;
			double avg_del = 0;
			double avg_ins = 0;
			vector<int> tmp = tmp_aln->get_avg_diff(dist, avg_del, avg_ins); //GrandOmics comment: refer to Alignment.cpp
			//	std::cout<<"Debug:\t"<<avg_del<<" "<<avg_ins<<endl;
			tot_avg_ins += avg_ins;
			tot_avg_del += avg_del;
			//
			avg_dist += dist;
			double avg_mis = 0;
			for (size_t i = 0; i < tmp.size(); i++) {
				while (tmp[i] + 1 > mis_per_window.size()) { //adjust length
					mis_per_window.push_back(0);
				}
				avg_mis += tmp[i];
				mis_per_window[tmp[i]]++;
			}
			//	std::cout <<avg_mis/tmp.size()<<"\t";
			//get score ratio
			double score = round(tmp_aln->get_scrore_ratio());
			//	std::cout<<score<<"\t"<<std::endl;;
			if (score > -1) {
				while (score + 1 > scores.size()) {
					scores.push_back(0);
				}
				scores[score]++;
			}
			num++;
		}

		mapped_file->parseReadFast(Parameter::Instance()->min_mq, tmp_aln);
	}
	if (num == 0) {
		std::cerr << "Too few reads detected in " << Parameter::Instance()->bam_files[0] << std::endl;
		exit(EXIT_FAILURE);
	}
	vector<int> nums;
	size_t pos = 0;
	Parameter::Instance()->max_dist_alns = floor(avg_dist / num) / 2;
	Parameter::Instance()->window_thresh = 50;			//25;
	if (!mis_per_window.empty()) {
		for (size_t i = 0; i < mis_per_window.size(); i++) {

			for (size_t j = 0; j < mis_per_window[i]; j++) {
				nums.push_back(i);
			}
		}
		pos = nums.size() * 0.95; //the highest 5% cutoff
		if (pos > 0 && pos <= nums.size()) {
			Parameter::Instance()->window_thresh = std::max(Parameter::Instance()->window_thresh, nums[pos]); //just in case we have too clean data! :)
		}
		nums.clear();
	}

	for (size_t i = 0; i < scores.size(); i++) {
		for (size_t j = 0; j < scores[i]; j++) {
			nums.push_back(i);
		}
	}
	pos = nums.size() * 0.05; //the lowest 5% cuttoff
	Parameter::Instance()->score_treshold = 2; //nums[pos]; //prev=2

	//cout<<"test: "<<tot_avg_ins<<" "<<num<<endl;
	//cout<<"test2: "<<tot_avg_del<<" "<<num<<endl;
	Parameter::Instance()->avg_del = tot_avg_del / num;
	Parameter::Instance()->avg_ins = tot_avg_ins / num;

	std::cout << "\tMax dist between aln events: " << Parameter::Instance()->max_dist_alns << std::endl;
	std::cout << "\tMax diff in window: " << Parameter::Instance()->window_thresh << std::endl;
	std::cout << "\tMin score ratio: " << Parameter::Instance()->score_treshold << std::endl;
	std::cout << "\tAvg DEL ratio: " << Parameter::Instance()->avg_del << std::endl;
	std::cout << "\tAvg INS ratio: " << Parameter::Instance()->avg_ins << std::endl;

}

bool overlaps(aln_str prev, aln_str curr) {

	double ratio = 0;
	double overlap = 0;
	if (prev.pos + Parameter::Instance()->min_length < curr.pos + curr.length && prev.pos + prev.length - Parameter::Instance()->min_length > curr.pos) {
		overlap = min((curr.pos + curr.length), (prev.pos + prev.length)) - max(prev.pos, curr.pos);
		ratio = overlap / (double) min(curr.length, prev.length);
	}
//	std::cout<<overlap<<" "<<ratio<<std::endl;
	return (ratio > 0.4 && overlap > 200);
}
