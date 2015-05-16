<?php
      
$list = array();
  
$files = glob("EPG/*");
foreach($files as $file) {
        
	$xml=simplexml_load_file($file);
                
        foreach($xml->programme as $program) { 
		$start = $program['start'];             
          
       
		$dt = new DateTime("@$start");

		if( in_array( $dt->format('d-m-Y'), $list)){ 
		
		}
		else{
			array_push($list,  $dt->format('d-m-Y'));
		}           
        }         
}  

sort($list);
foreach ($list as $value) {
    echo "<option value=". $value ."   >" . $value . "</option>";
}
   
?>